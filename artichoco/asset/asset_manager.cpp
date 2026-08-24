#include "asset_manager.h"

#include "asset_log.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <ranges>
#include <system_error>

namespace arti::asset {
namespace {

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

struct OutputIdentity {
    std::filesystem::path source;
    AssetType type;
    bool operator==(const OutputIdentity&) const = default;
};

struct OutputIdentityHash {
    size_t operator()(const OutputIdentity& identity) const noexcept {
        const size_t source_hash = std::hash<std::filesystem::path>{}(identity.source);
        return source_hash ^ (std::hash<AssetType>{}(identity.type) << 1);
    }
};

}

bool AssetManager::open(std::filesystem::path assets_root, std::filesystem::path artifacts_root) {
    close();
    if (!m_storage.open(std::move(assets_root), std::move(artifacts_root))) {
        return false;
    }
    const auto metadata = m_storage.scanMetadata();
    if (!metadata) {
        m_storage.close();
        return false;
    }
    for (const AssetMetadata& entry: *metadata) {
        m_catalog.insert(entry);
    }
    getLogChannel().info("Opened AssetManager with {} Assets", m_catalog.importedCount());
    return true;
}

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
    for (const ImporterEntry& entry: m_importers) {
        const auto conflict = std::ranges::find_first_of(entry.extensions, extensions);
        if (conflict != entry.extensions.end()) {
            getLogChannel().error(
                    "An AssetImporter for '{}' is already registered: every .meta sidecar "
                    "belongs to exactly one source extension",
                    *conflict);
            return false;
        }
    }

    importer->setWorkspace(m_storage, m_catalog);
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

std::vector<AssetImportResult> AssetManager::import(const std::filesystem::path& source_path,
        const AssetImporter& importer) {
    std::vector<AssetImportResult> results;
    if (!m_storage.isOpen()) {
        getLogChannel().error("Cannot import '{}': the Asset workspace is not open",
                source_path.string());
        return results;
    }

    const auto entry = std::ranges::find_if(m_importers,
            [&importer](const ImporterEntry& entry) { return entry.importer.get() == &importer; });
    if (entry == m_importers.end()) {
        getLogChannel().error("Cannot import '{}': the AssetImporter is not registered",
                source_path.string());
        return results;
    }

    const std::filesystem::path normalized_source = source_path.lexically_normal();
    const auto source_file = m_storage.resolveSourcePath(normalized_source);
    std::error_code error;
    if (!source_file || !std::filesystem::is_regular_file(*source_file, error)) {
        getLogChannel().error("Source file '{}' does not exist: {}", normalized_source.string(),
                error ? error.message() : "unsafe or unresolved path");
        return results;
    }

    AssetImportResult result;
    try {
        result = entry->importer->import(normalized_source);
    } catch (const std::exception& exception) {
        result.error = std::string{ "importer threw: " } + exception.what();
    } catch (...) {
        result.error = "importer threw an unknown exception";
    }

    if (result) {
        importOutputs(result.outputs, normalized_source);
    } else {
        getLogChannel().error("Import of '{}' failed: {}", normalized_source.string(),
                result.error);
    }
    results.push_back(std::move(result));
    return results;
}

void AssetManager::importOutputs(std::vector<AssetImportOutput>& outputs,
        const std::filesystem::path& normalized_source) {
    std::unordered_map<OutputIdentity, core::UUID, OutputIdentityHash> existing_by_identity;
    for (const AssetImportOutput& output: outputs) {
        std::filesystem::path output_source = normalized_source;
        if (!output.source_suffix.empty()) {
            output_source += output.source_suffix;
        }
        const OutputIdentity identity{ output_source, output.metadata.type };
        if (output.already_imported || existing_by_identity.contains(identity)) {
            continue;
        }
        if (const auto existing =
                        m_catalog.findBySourcePathAndType(output_source, output.metadata.type)) {
            existing_by_identity.emplace(identity, existing->handle);
        }
    }

    for (AssetImportOutput& output: outputs) {
        std::filesystem::path output_source = normalized_source;
        if (!output.source_suffix.empty()) {
            output_source += output.source_suffix;
        }
        output.metadata.source_path = output_source;

        if (output.already_imported) {
            if (!isValidAssetMetadata(output.metadata)) {
                getLogChannel().error("Existing metadata is invalid while importing '{}'",
                        normalized_source.string());
                continue;
            }
            m_catalog.insert(output.metadata);
            getLogChannel().info("Reused existing '{}' as {} ({})", normalized_source.string(),
                    output.metadata.type, output.metadata.handle.toString());
            continue;
        }

        const OutputIdentity identity{ output_source, output.metadata.type };
        if (const auto existing = existing_by_identity.find(identity);
                existing != existing_by_identity.end()) {
            unloadWithDependents(existing->second);
            output.metadata.handle = existing->second;
            existing_by_identity.erase(existing);
        }

        if (!isValidAssetMetadata(output.metadata)) {
            getLogChannel().error("Importer returned invalid metadata while importing '{}'",
                    normalized_source.string());
            continue;
        }
        if (!m_storage.writeArtifact(output.metadata.artifact_path, output.encoded)) {
            getLogChannel().error("Failed to write an artifact while importing '{}'",
                    normalized_source.string());
            continue;
        }
        if (!m_storage.writeMetadata(output.metadata)) {
            getLogChannel().error("Failed to write metadata while importing '{}'",
                    normalized_source.string());
            continue;
        }
        m_catalog.insert(output.metadata);
        getLogChannel().info("Imported '{}' as {} ({})", normalized_source.string(),
                output.metadata.type, output.metadata.handle.toString());
    }
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
