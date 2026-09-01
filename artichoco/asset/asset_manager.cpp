#include "asset_manager.h"

#include "asset_log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>
#include <ranges>
#include <system_error>
#include <tuple>
#include <unordered_set>

namespace arti::asset {
namespace {

bool readTextFile(const std::filesystem::path& file, std::string& contents) {
    std::ifstream input{ file, std::ios::binary };
    if (!input.is_open()) {
        return false;
    }
    contents.assign(std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

std::string normalizeExtension(std::string_view extension) {
    if (extension.empty()) {
        return {};
    }
    std::string normalized{ extension };
    if (normalized.front() != '.') {
        normalized.insert(normalized.begin(), '.');
    }
    std::ranges::transform(normalized, normalized.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return normalized == "." ? std::string{} : normalized;
}

std::string extensionOf(const std::filesystem::path& path) {
    return normalizeExtension(path.extension().string());
}

} // namespace

size_t ReconcilePlan::countWithAction(ReconcileAction action) const {
    return static_cast<size_t>(std::ranges::count_if(items,
            [action](const ReconcileItem& item) { return item.action == action; }));
}

bool ReconcilePlan::hasWork() const {
    return countWithAction(ReconcileAction::Import) > 0 ||
           countWithAction(ReconcileAction::Reimport) > 0 || !orphans.empty();
}

bool AssetManager::open(std::filesystem::path assets_root, std::filesystem::path artifacts_root) {
    close();
    if (!m_storage.open(std::move(assets_root), std::move(artifacts_root))) {
        return false;
    }

    const MetadataScan scan = m_storage.scanMetadata();
    if (!scan.complete()) {
        // 扫描不完整意味着我们看不到全部 .meta。此时继续会让 reconcile 把
        // 没看到的条目当成"不存在"，从而删掉不该删的东西。宁可开不了项目。
        getLogChannel().error("Refusing to open AssetManager: the metadata scan is incomplete ({})",
                scan.traversal_error);
        m_storage.close();
        return false;
    }

    size_t conflicts = 0;
    for (const SourceMetadata& sidecar: scan.entries) {
        for (const AssetMetadata& entry: expandSourceMetadata(sidecar)) {
            if (m_catalog.insert(entry, AssetOrigin::User) == AssetInsertStatus::Conflicted) {
                ++conflicts;
            }
        }
    }
    getLogChannel().info("Opened AssetManager with {} Asset(s)", m_catalog.importedCount());
    if (!scan.issues.empty() || conflicts > 0) {
        getLogChannel().warn(
                "The Assets root has {} rejected metadata file(s) and {} UUID conflict(s); "
                "run a reconcile for details",
                scan.issues.size(), conflicts);
    }
    return true;
}

bool AssetManager::openPackaged(std::filesystem::path artifacts_root,
        const std::filesystem::path& manifest_file) {
    close();
    if (!m_storage.openArtifactsOnly(std::move(artifacts_root))) {
        return false;
    }

    std::string text;
    if (!readTextFile(manifest_file, text)) {
        getLogChannel().error("Failed to read the Asset manifest '{}'", manifest_file.string());
        m_storage.close();
        return false;
    }
    const auto manifest = deserializeAssetManifest(text);
    if (!manifest) {
        // deserializeAssetManifest 已经记了具体原因。坏 manifest 就该让游戏起不来 ——
        // 少几个资产悄悄接着跑，表现是「东西凭空消失」，比启动失败难查得多。
        m_storage.close();
        return false;
    }

    for (const AssetMetadata& entry: manifest->assets) {
        // manifest 那边已经拒过重复 handle，而 Engine 条目此刻还没登记，所以这里不会冲突。
        m_catalog.insert(entry, AssetOrigin::User);
    }
    getLogChannel().info("Opened AssetManager from the manifest '{}' with {} Asset(s)",
            manifest_file.string(), m_catalog.importedCount());
    return true;
}

AssetManifest AssetManager::buildManifest() const {
    AssetManifest manifest;
    const std::vector<AssetEntry> entries = m_catalog.entriesWithOrigin(AssetOrigin::User);
    manifest.assets.reserve(entries.size());
    for (const AssetEntry& entry: entries) {
        manifest.assets.push_back(entry.metadata);
    }
    std::ranges::sort(manifest.assets,
            [](const AssetMetadata& left, const AssetMetadata& right) {
                return left.handle.value() < right.handle.value();
            });
    return manifest;
}

// 注意：importer 与 loader 的注册**不**在这里清理。它们不绑定工作区
// （setWorkspace 拿的是成员引用），允许在 open() 之前注册，而 open() 会先调 close()。
void AssetManager::close() noexcept {
    m_storage.close();
    m_catalog.clear();
    m_loaded.clear();
    m_loading.clear();
}

bool AssetManager::registerImporter(std::unique_ptr<AssetImporter> importer) {
    if (!importer) {
        getLogChannel().error("Cannot register a null AssetImporter");
        return false;
    }

    std::vector<std::string> extensions;
    try {
        for (const std::string& extension: importer->getSupportedExtensions()) {
            std::string normalized = normalizeExtension(extension);
            if (!normalized.empty() &&
                    std::ranges::find(extensions, normalized) == extensions.end()) {
                extensions.push_back(std::move(normalized));
            }
        }
    } catch (...) {
        getLogChannel().error("AssetImporter threw while declaring its supported extensions");
        return false;
    }
    if (extensions.empty()) {
        getLogChannel().error("AssetImporter did not declare any supported extensions");
        return false;
    }
    // 先全部查冲突再改状态，避免被拒绝的 importer 留下半套映射。
    for (const std::string& extension: extensions) {
        if (m_importer_by_extension.contains(extension)) {
            getLogChannel().error(
                    "An AssetImporter for '{}' is already registered: every .meta sidecar "
                    "belongs to exactly one source extension",
                    extension);
            return false;
        }
    }

    importer->setWorkspace(m_storage, m_catalog);
    auto* registered = importer.get();
    for (const std::string& extension: extensions) {
        m_importer_by_extension.emplace(extension, registered);
    }
    m_importers.push_back({ std::move(importer), std::move(extensions) });
    getLogChannel().info("Registered AssetImporter for {} extension(s)",
            m_importers.back().extensions.size());
    return true;
}

bool AssetManager::registerLoader(std::unique_ptr<AssetLoader> loader) {
    if (!loader) {
        getLogChannel().error("Cannot register a null AssetLoader");
        return false;
    }

    AssetType type;
    try {
        type = loader->getType();
    } catch (...) {
        getLogChannel().error("AssetLoader threw while declaring its type");
        return false;
    }
    if (type.empty()) {
        getLogChannel().error("Cannot register an AssetLoader with an empty type");
        return false;
    }
    if (m_loaders.contains(type)) {
        getLogChannel().error("An AssetLoader is already registered for '{}'", type);
        return false;
    }

    m_loaders.emplace(type, std::move(loader));
    getLogChannel().info("Registered AssetLoader for '{}'", type);
    return true;
}

void AssetManager::registerEngineAssetProvider(EngineAssetProvider provider) {
    if (provider) {
        m_engine_providers.push_back(std::move(provider));
    }
}

const AssetImporter* AssetManager::importerFor(const std::filesystem::path& source_path) const {
    if (!isSafeAssetRelativePath(source_path)) {
        return nullptr;
    }
    const auto found = m_importer_by_extension.find(extensionOf(source_path));
    return found == m_importer_by_extension.end() ? nullptr : found->second;
}

bool AssetManager::canImport(const std::filesystem::path& source_path) const {
    return importerFor(source_path) != nullptr;
}

AssetImportResult AssetManager::import(const std::filesystem::path& source_path) {
    const AssetImporter* importer = importerFor(source_path);
    if (importer == nullptr) {
        AssetImportResult result;
        result.error = "no importer supports this source extension";
        return result;
    }
    return import(source_path, *importer);
}

AssetImportResult AssetManager::import(const std::filesystem::path& source_path,
        const AssetImporter& importer) {
    AssetImportResult result;
    if (!m_storage.isOpen()) {
        result.error = "the Asset workspace is not open";
        getLogChannel().error("Cannot import '{}': {}", source_path.string(), result.error);
        return result;
    }
    if (!m_storage.hasSources()) {
        result.error = "the Asset workspace is packaged and has no source tree";
        getLogChannel().error("Cannot import '{}': {}", source_path.string(), result.error);
        return result;
    }

    const auto entry = std::ranges::find_if(m_importers,
            [&importer](const ImporterEntry& entry) { return entry.importer.get() == &importer; });
    if (entry == m_importers.end()) {
        result.error = "the AssetImporter is not registered";
        getLogChannel().error("Cannot import '{}': {}", source_path.string(), result.error);
        return result;
    }

    const std::filesystem::path normalized_source = source_path.lexically_normal();
    if (!m_storage.hasSource(normalized_source)) {
        result.error = "the source file does not exist";
        getLogChannel().error("Cannot import '{}': {}", normalized_source.string(), result.error);
        return result;
    }

    // 沿用上一份 sidecar 的设置（Authored 是用户数据），解析成有效值再交给 importer。
    AssetSettings stored;
    if (const auto previous = m_storage.readMetadata(normalized_source)) {
        stored = previous->settings;
    }
    const ResolvedSettings settings = resolveSettings(entry->importer->getSettingSchema(), stored);
    for (const SettingIssue& issue: settings.issues()) {
        getLogChannel().warn("Setting '{}' on '{}': {}", issue.key,
                normalized_source.generic_string(), issue.detail);
    }

    AssetImportRequest request;
    request.source_path = normalized_source;
    request.settings = &settings;

    try {
        result = entry->importer->import(request);
    } catch (const std::exception& exception) {
        result = {};
        result.error = std::string{ "importer threw: " } + exception.what();
    } catch (...) {
        result = {};
        result.error = "importer threw an unknown exception";
    }

    if (result) {
        if (!commitOutputs(normalized_source, *entry->importer, stored, settings,
                    result.outputs)) {
            result.error = "failed to commit the import outputs";
        }
    } else {
        getLogChannel().error("Import of '{}' failed: {}", normalized_source.string(),
                result.error);
    }
    return result;
}

// 一源一 sidecar：先把全部产出攒成一份 SourceMetadata，artifact 全部写成功后
// 才写 sidecar。中途失败不留半套 —— 旧 sidecar 保持原样，下一轮 reconcile 重试。
bool AssetManager::commitOutputs(const std::filesystem::path& normalized_source,
        const AssetImporter& importer, const AssetSettings& stored,
        const ResolvedSettings& resolved, std::vector<AssetImportOutput>& outputs) {
    SourceMetadata sidecar;
    sidecar.version = kSourceMetadataVersion;
    sidecar.source_path = normalized_source;
    sidecar.importer.name = importer.getName();
    sidecar.importer.version = importer.getVersion();
    // 指纹槽位：只写不读，给以后的源变更检测留位置，避免二次格式变更。
    if (const auto size = m_storage.sourceSize(normalized_source)) {
        sidecar.fingerprint.size = *size;
    }

    // 原样写回 Authored/Inferred —— 解析后的有效值绝不能写进 Authored，
    // 否则"键存在"这个信号就被污染了，下次推断再也无法生效。
    sidecar.settings = stored;
    sidecar.settings_hash = resolved.hash();

    for (AssetImportOutput& output: outputs) {
        // identity 复用：(source_path, local_id) 命中就沿用既有 handle，
        // 场景里的引用因此在重导入后仍然有效。
        if (const auto existing =
                        m_catalog.findBySourceAndLocalId(normalized_source, output.record.local_id)) {
            if (!output.already_imported) {
                unloadWithDependents(existing->handle);
            }
            output.record.handle = existing->handle;
        }
        if (!output.record.handle.isValid()) {
            output.record.handle = core::UUID::generate();
        }
        if (output.record.artifact_path.empty()) {
            output.record.artifact_path = std::filesystem::path{ "Imported" } /
                                          output.record.handle.toString();
        }
        sidecar.assets.push_back(output.record);
    }

    if (!isValidSourceMetadata(sidecar)) {
        getLogChannel().error("Importer returned invalid metadata while importing '{}'",
                normalized_source.string());
        return false;
    }

    for (const AssetImportOutput& output: outputs) {
        if (output.already_imported) {
            continue;
        }
        if (!m_storage.writeArtifact(output.record.artifact_path, output.encoded)) {
            getLogChannel().error("Failed to write an artifact while importing '{}'",
                    normalized_source.string());
            return false;
        }
    }
    if (!m_storage.writeMetadata(sidecar)) {
        getLogChannel().error("Failed to write metadata while importing '{}'",
                normalized_source.string());
        return false;
    }

    // sidecar 落盘成功之后才动 catalog，保证内存与磁盘一致。
    //
    // 这一轮不再产出的条目必须从 catalog 里删掉 —— 否则重导入之后内存里会留下
    // 磁盘上已经不存在的资产。Extract 就会触发这种情况：材质被提取走之后，
    // 容器不再产出它，但那条旧记录会一直粘到下一次全量 reconcile。
    for (const core::UUID handle: m_catalog.handlesForSource(normalized_source)) {
        const bool still_produced = std::ranges::any_of(sidecar.assets,
                [handle](const AssetRecord& record) { return record.handle == handle; });
        if (!still_produced) {
            unloadWithDependents(handle);
            m_catalog.erase(handle);
        }
    }
    for (const AssetMetadata& entry: expandSourceMetadata(sidecar)) {
        m_catalog.insert(entry, AssetOrigin::User);
    }
    getLogChannel().info("Imported '{}' ({} asset(s))", normalized_source.string(),
            sidecar.assets.size());
    return true;
}

ReconcilePlan AssetManager::planReconcile() const {
    ReconcilePlan plan;
    if (!m_storage.isOpen()) {
        plan.traversal_error = "the Asset workspace is not open";
        return plan;
    }
    if (!m_storage.hasSources()) {
        plan.traversal_error = "the Asset workspace is packaged and has no source tree";
        return plan;
    }

    const SourceScan sources = m_storage.scanSources();
    const MetadataScan metadata = m_storage.scanMetadata();
    plan.metadata_issues = metadata.issues;
    if (!sources.complete()) {
        plan.traversal_error = sources.traversal_error;
        return plan;
    }
    if (!metadata.complete()) {
        plan.traversal_error = metadata.traversal_error;
        return plan;
    }

    std::unordered_set<std::string> real_files;
    real_files.reserve(sources.files.size());
    for (const SourceFile& file: sources.files) {
        real_files.insert(file.relative_path.lexically_normal().generic_string());
    }

    // handle → 已被哪个 source_path 占用，用于检测 UUID 冲突。
    std::unordered_map<core::UUID, std::filesystem::path> claimed;
    // 源文件 → 它产出的资产。一源一 sidecar，所以这是直接分组，不需要回溯。
    std::unordered_map<std::string, std::vector<AssetMetadata>> owned;

    for (const SourceMetadata& sidecar: metadata.entries) {
        const std::string key = sidecar.source_path.lexically_normal().generic_string();
        const bool source_exists = real_files.contains(key);

        std::vector<AssetMetadata> accepted;
        for (const AssetMetadata& entry: expandSourceMetadata(sidecar)) {
            const auto claim = claimed.find(entry.handle);
            if (claim != claimed.end()) {
                plan.conflicts.push_back({ entry.handle, claim->second, entry.source_path });
                continue;
            }
            claimed.emplace(entry.handle, entry.source_path);
            accepted.push_back(entry);
        }

        if (!source_exists) {
            // 源文件不在了 —— 整份 sidecar 连同它的 artifact 都是派生数据。
            for (const AssetMetadata& entry: accepted) {
                plan.orphans.push_back({ entry, sidecar.source_path });
            }
            continue;
        }

        // 只保留被接受的记录（冲突条目已剔除）。
        SourceMetadata kept = sidecar;
        kept.assets.clear();
        for (const AssetMetadata& entry: accepted) {
            for (const AssetRecord& record: sidecar.assets) {
                if (record.handle == entry.handle) {
                    kept.assets.push_back(record);
                    break;
                }
            }
        }
        plan.accepted_metadata.push_back(std::move(kept));
        owned.emplace(key, std::move(accepted));
    }

    plan.items.reserve(sources.files.size());
    for (const SourceFile& file: sources.files) {
        ReconcileItem item;
        item.source_path = file.relative_path.lexically_normal();

        if (const auto found = owned.find(item.source_path.generic_string());
                found != owned.end()) {
            item.existing = found->second;
        }

        if (!canImport(item.source_path)) {
            item.action = ReconcileAction::Unsupported;
        } else if (item.existing.empty()) {
            item.action = ReconcileAction::Import;
        } else {
            item.action = ReconcileAction::Current;
            for (const AssetMetadata& asset: item.existing) {
                if (!m_storage.hasArtifact(asset.artifact_path)) {
                    item.action = ReconcileAction::Reimport;
                    item.reason = "missing artifact: " + asset.artifact_path.generic_string();
                    break;
                }
            }
        }
        plan.items.push_back(std::move(item));
    }

    // 先按字典序，让后面的裁决和拓扑排序都是确定的。
    std::ranges::sort(plan.items, [](const ReconcileItem& left, const ReconcileItem& right) {
        return left.source_path.generic_string() < right.source_path.generic_string();
    });

    collectInferences(plan, real_files);
    orderByDependencies(plan);
    std::ranges::sort(plan.orphans, [](const ReconcileOrphan& left, const ReconcileOrphan& right) {
        return left.metadata.source_path.generic_string() <
               right.metadata.source_path.generic_string();
    });
    return plan;
}

// 跑一遍 prescan，把容器对被引用源文件的设置推断收集起来并裁决冲突。
// 结果写进 plan.items[].inferred，apply() 在导入前落盘。
void AssetManager::collectInferences(ReconcilePlan& plan,
        const std::unordered_set<std::string>& real_files) const {
    // target → key → 候选（按发布者路径排序后第一个胜出）
    struct Candidate {
        std::filesystem::path publisher;
        InferredSetting setting;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Candidate>>>
            candidates;

    for (ReconcileItem& item: plan.items) {
        const AssetImporter* importer = importerFor(item.source_path);
        if (importer == nullptr) {
            continue;
        }
        SourcePrescan prescan;
        try {
            prescan = importer->prescan(item.source_path);
        } catch (...) {
            continue; // prescan 失败等价于什么都没声明
        }

        for (const std::filesystem::path& reference: prescan.referenced_sources) {
            const std::string key = reference.lexically_normal().generic_string();
            if (real_files.contains(key)) {
                item.references.push_back(reference.lexically_normal());
            }
        }
        for (const InferenceSuggestion& suggestion: prescan.suggestions) {
            const std::string target = suggestion.target_source.lexically_normal().generic_string();
            if (!real_files.contains(target)) {
                continue;
            }
            candidates[target][suggestion.key].push_back(
                    { item.source_path, { suggestion.value, item.source_path, suggestion.usage } });
        }
    }

    for (auto& [target, by_key]: candidates) {
        for (auto& [key, list]: by_key) {
            std::ranges::sort(list, [](const Candidate& left, const Candidate& right) {
                return left.publisher.generic_string() < right.publisher.generic_string();
            });
            const Candidate& winner = list.front();
            for (size_t index = 1; index < list.size(); ++index) {
                if (list[index].setting.value == winner.setting.value) {
                    continue; // 同一个值，不算冲突
                }
                plan.inference_conflicts.push_back({ std::filesystem::path{ target }, key,
                    winner.publisher, list[index].publisher, winner.setting.usage,
                    list[index].setting.usage });
            }

            const auto item = std::ranges::find_if(plan.items,
                    [&target](const ReconcileItem& candidate) {
                        return candidate.source_path.generic_string() == target;
                    });
            if (item != plan.items.end()) {
                item->inferred.emplace(key, winner.setting);
            }
        }
    }

    // 推断变了必须重导入，否则新设置永远不生效。用 settings_hash 判断：
    // 把本轮推断投影进设置再解析一遍，和 sidecar 里存的哈希比。
    // 这是 settings_hash 从"只写"变成真正被读的地方；源内容哈希和 importer
    // 版本仍然只写不读（那两条排在多线程之后）。
    for (ReconcileItem& item: plan.items) {
        if (item.action != ReconcileAction::Current && item.action != ReconcileAction::Import) {
            continue;
        }
        const AssetImporter* importer = importerFor(item.source_path);
        if (importer == nullptr) {
            continue;
        }

        AssetSettings projected;
        uint64_t stored_hash = 0;
        if (const auto sidecar = m_storage.readMetadata(item.source_path)) {
            projected = sidecar->settings;
            stored_hash = sidecar->settings_hash;
        }
        projected.inferred = item.inferred;

        const uint64_t resolved =
                resolveSettings(importer->getSettingSchema(), projected).hash();
        if (item.action == ReconcileAction::Current && resolved != stored_hash) {
            item.action = ReconcileAction::Reimport;
            item.reason = "import settings changed";
        }
    }
}

// 被引用者排在引用者之前：贴图先导入，glTF 才能引用它们的产出。
// 深度优先 + 灰/黑标记，成环时按字典序破环并报告。
void AssetManager::orderByDependencies(ReconcilePlan& plan) const {
    std::unordered_map<std::string, size_t> index_of;
    for (size_t index = 0; index < plan.items.size(); ++index) {
        index_of.emplace(plan.items[index].source_path.generic_string(), index);
    }

    enum class Mark : uint8_t { White, Grey, Black };
    std::vector<Mark> marks(plan.items.size(), Mark::White);
    std::vector<ReconcileItem> ordered;
    ordered.reserve(plan.items.size());

    const auto visit = [&](size_t start, auto&& self) -> void {
        if (marks[start] == Mark::Black) {
            return;
        }
        if (marks[start] == Mark::Grey) {
            plan.dependency_cycles.push_back(plan.items[start].source_path);
            return;
        }
        marks[start] = Mark::Grey;
        for (const std::filesystem::path& reference: plan.items[start].references) {
            const auto found = index_of.find(reference.generic_string());
            if (found != index_of.end() && found->second != start) {
                self(found->second, self);
            }
        }
        marks[start] = Mark::Black;
        ordered.push_back(plan.items[start]);
    };

    // plan.items 已按字典序排好，所以遍历起点是确定的。
    for (size_t index = 0; index < plan.items.size(); ++index) {
        visit(index, visit);
    }
    plan.items = std::move(ordered);
}

void AssetManager::rebuildUserCatalog(const std::vector<SourceMetadata>& metadata) {
    m_catalog.eraseOrigin(AssetOrigin::User);
    for (const SourceMetadata& sidecar: metadata) {
        for (const AssetMetadata& entry: expandSourceMetadata(sidecar)) {
            m_catalog.insert(entry, AssetOrigin::User);
        }
    }
}

// 把本轮裁决出的推断落进 sidecar，让 import() 能通过 Settings.Inferred 读到。
// 首次导入时源文件还没有 sidecar，所以要先建一份只带 settings 的空壳；
// commitOutputs 之后它会被完整的 sidecar 覆盖。
bool AssetManager::persistInferences(const ReconcileItem& item) {
    if (item.inferred.empty()) {
        return true;
    }
    SourceMetadata sidecar;
    if (const auto existing = m_storage.readMetadata(item.source_path)) {
        sidecar = *existing;
    } else {
        sidecar.version = kSourceMetadataVersion;
        sidecar.source_path = item.source_path;
    }

    bool changed = false;
    for (const auto& [key, setting]: item.inferred) {
        const auto found = sidecar.settings.inferred.find(key);
        if (found == sidecar.settings.inferred.end() || !(found->second == setting)) {
            sidecar.settings.inferred[key] = setting;
            changed = true;
        }
    }
    // 本轮不再被任何容器推断的键要清掉，否则旧推断会永久粘住。
    for (auto it = sidecar.settings.inferred.begin(); it != sidecar.settings.inferred.end();) {
        if (!item.inferred.contains(it->first)) {
            it = sidecar.settings.inferred.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (!changed) {
        return true;
    }
    return m_storage.writeMetadata(sidecar);
}

bool AssetManager::importOne(const std::filesystem::path& source_path, ReconcileReport& report) {
    const AssetImportResult result = import(source_path);
    if (!result) {
        ++report.failed;
        report.errors.push_back(source_path.generic_string() + ": " + result.error);
        return false;
    }
    return true;
}

ReconcileReport AssetManager::applyReconcile(const ReconcilePlan& plan) {
    ReconcileReport report;
    if (!m_storage.isOpen()) {
        report.errors.emplace_back("the Asset workspace is not open");
        return report;
    }
    if (!m_storage.hasSources()) {
        report.errors.emplace_back("the Asset workspace is packaged and has no source tree");
        return report;
    }
    if (!plan.complete()) {
        report.errors.push_back("the reconcile plan is incomplete: " + plan.traversal_error);
        return report;
    }

    // Engine 资产没有源文件，reconcile 的三方对账管不到它们，
    // 所以先让提供者自己补齐缺失的 artifact。
    for (const EngineAssetProvider& provider: m_engine_providers) {
        if (!provider(*this)) {
            report.errors.emplace_back("an engine asset provider failed to restore its assets");
        }
    }

    // catalog 的 User 集合是磁盘 .meta 的纯函数。Engine 条目不受影响。
    rebuildUserCatalog(plan.accepted_metadata);

    // 孤儿：源文件没了，.meta 和 artifact 都是派生数据，一起清掉。
    std::unordered_set<std::string> removed_meta;
    std::unordered_set<std::string> removed_artifacts;
    for (const ReconcileOrphan& orphan: plan.orphans) {
        const AssetMetadata& metadata = orphan.metadata;
        unloadWithDependents(metadata.handle);
        // rebuildUserCatalog 已经把它们清出 catalog（accepted_metadata 不含孤儿），
        // 这里的 erase 只是防御性的，计数直接按孤儿条目数记。
        m_catalog.erase(metadata.handle);
        ++report.forgotten;

        // 一源一 sidecar：同一个源的多个孤儿资产共用一份 .meta，只删一次。
        const std::string meta_key =
                AssetStorage::metadataPathFor(orphan.owning_source).generic_string();
        if (removed_meta.insert(meta_key).second) {
            if (m_storage.removeMetadata(orphan.owning_source)) {
                ++report.removed_metadata;
            } else {
                report.errors.push_back("failed to remove metadata for " +
                                        orphan.owning_source.generic_string());
            }
        }
        const std::string artifact_key = metadata.artifact_path.generic_string();
        if (removed_artifacts.insert(artifact_key).second) {
            if (m_storage.removeArtifact(metadata.artifact_path)) {
                ++report.removed_artifacts;
            } else {
                report.errors.push_back("failed to remove artifact " + artifact_key);
            }
        }
        getLogChannel().info("Forgot orphaned Asset {} ('{}')", metadata.handle.toString(),
                metadata.source_path.generic_string());
    }

    // plan.items 已按拓扑序排好：被引用者（贴图）在引用者（glTF）之前。
    for (const ReconcileItem& item: plan.items) {
        // 推断先落盘，import() 才能通过 Settings.Inferred 读到。
        if (!persistInferences(item)) {
            report.errors.push_back("failed to persist inferred settings for " +
                                    item.source_path.generic_string());
        }

        switch (item.action) {
        case ReconcileAction::Import:
            if (importOne(item.source_path, report)) {
                ++report.imported;
            }
            break;
        case ReconcileAction::Reimport:
            getLogChannel().info("Reimporting '{}': {}", item.source_path.generic_string(),
                    item.reason);
            if (importOne(item.source_path, report)) {
                ++report.reimported;
            }
            break;
        case ReconcileAction::Current:
            ++report.current;
            break;
        case ReconcileAction::Unsupported:
            ++report.unsupported;
            break;
        }
    }

    getLogChannel().info(
            "Reconcile finished: {} imported, {} reimported, {} current, {} unsupported, "
            "{} failed, {} forgotten",
            report.imported, report.reimported, report.current, report.unsupported, report.failed,
            report.forgotten);
    return report;
}

ReconcileReport AssetManager::reconcile() { return applyReconcile(planReconcile()); }

AssetIntegrityReport AssetManager::checkIntegrity() const {
    AssetIntegrityReport report;
    if (!m_storage.isOpen()) {
        report.issues.push_back({ {}, "the Asset workspace is not open" });
        return report;
    }

    const std::vector<AssetEntry> entries = m_catalog.allEntries();
    report.assets_checked = entries.size();
    for (const AssetEntry& entry: entries) {
        if (!m_storage.hasArtifact(entry.metadata.artifact_path)) {
            report.issues.push_back({ entry.metadata.handle,
                "missing artifact: " + entry.metadata.artifact_path.generic_string() });
        }
        for (const core::UUID dependency: entry.metadata.dependencies) {
            if (!m_catalog.find(dependency)) {
                report.issues.push_back({ entry.metadata.handle,
                    "missing dependency: " + dependency.toString() });
            }
        }
    }
    std::ranges::sort(report.issues,
            [](const AssetIntegrityIssue& left, const AssetIntegrityIssue& right) {
                return std::tuple{ left.handle.value(), left.message } <
                       std::tuple{ right.handle.value(), right.message };
            });
    return report;
}

std::shared_ptr<Asset> AssetManager::load(core::UUID handle) {
    if (!handle.isValid() || !m_storage.isOpen()) {
        getLogChannel().error("Cannot load Asset {}: invalid handle or closed workspace",
                handle.toString());
        return {};
    }
    if (std::shared_ptr<Asset> asset = getAsset(handle)) {
        getLogChannel().debug("Using cached Asset {}", handle.toString());
        return asset;
    }
    if (m_loading.contains(handle)) {
        getLogChannel().error("Cycle detected while loading Asset {}", handle.toString());
        return {};
    }
    const auto metadata = m_catalog.find(handle);
    if (!metadata) {
        getLogChannel().error("Asset {} is not present in the AssetCatalog", handle.toString());
        return {};
    }
    const auto loader = m_loaders.find(metadata->type);
    if (loader == m_loaders.end()) {
        getLogChannel().error("No AssetLoader is registered for '{}'", metadata->type);
        return {};
    }
    const auto artifact_file = m_storage.resolveArtifactPath(metadata->artifact_path);
    if (!artifact_file) {
        getLogChannel().error("Asset {} contains an invalid Artifact path", handle.toString());
        return {};
    }

    m_loading.insert(handle);
    std::shared_ptr<Asset> asset;
    try {
        std::vector<std::shared_ptr<Asset>> dependencies;
        dependencies.reserve(metadata->dependencies.size());
        bool dependencies_complete = true;
        for (core::UUID dependency: metadata->dependencies) {
            std::shared_ptr<Asset> loaded = load(dependency);
            if (!loaded) {
                getLogChannel().error("Failed to load dependency {} of Asset {}",
                        dependency.toString(), handle.toString());
                dependencies_complete = false;
                break;
            }
            dependencies.push_back(std::move(loaded));
        }
        if (dependencies_complete) {
            asset = loader->second->decode(*metadata, *artifact_file, dependencies);
        }
    } catch (const std::exception& exception) {
        getLogChannel().error("AssetLoader threw while decoding Asset {}: {}", handle.toString(),
                exception.what());
    } catch (...) {
        getLogChannel().error("AssetLoader threw while decoding Asset {}", handle.toString());
    }
    m_loading.erase(handle);

    if (asset == nullptr) {
        getLogChannel().error("AssetLoader failed to decode the Artifact '{}' for Asset {}",
                artifact_file->string(), handle.toString());
        return {};
    }
    if (asset->getHandle() != handle || asset->getType() != metadata->type) {
        getLogChannel().error("AssetLoader decoded an Asset with the wrong identity or type");
        return {};
    }
    m_loaded.insert_or_assign(handle, asset);
    getLogChannel().info("Loaded Asset {} ({})", handle.toString(), metadata->type);
    return asset;
}

std::shared_ptr<Asset> AssetManager::getAsset(core::UUID handle) const noexcept {
    const auto found = m_loaded.find(handle);
    return found == m_loaded.end() ? nullptr : found->second.lock();
}

void AssetManager::unloadWithDependents(core::UUID handle) noexcept {
    std::vector<core::UUID> stack{ handle };
    std::unordered_set<core::UUID> visited;
    while (!stack.empty()) {
        const core::UUID current = stack.back();
        stack.pop_back();
        if (!visited.insert(current).second) {
            continue;
        }
        m_loaded.erase(current);
        for (core::UUID dependent: m_catalog.dependentsOf(current)) {
            stack.push_back(dependent);
        }
    }
}

}
