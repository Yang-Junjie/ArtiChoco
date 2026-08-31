#pragma once

#include "asset_catalog.h"
#include "asset_importer.h"
#include "asset_loader.h"
#include "asset_reconcile.h"
#include "asset_storage.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arti::asset {

class AssetManager {
public:
    bool open(std::filesystem::path assets_root, std::filesystem::path artifacts_root);
    void close() noexcept;

    bool registerImporter(std::unique_ptr<AssetImporter> importer);
    bool registerLoader(std::unique_ptr<AssetLoader> loader);

    // 注册 Engine 来源资产的提供者。它负责把自己的条目登记进 catalog，
    // 并保证对应 artifact 存在（缺了就重新生成）。
    // applyReconcile() 每次都会调用它，让 "Library/ 可删可重建" 对
    // Engine 资产同样成立 —— 它们没有源文件，reconcile 本身修不了。
    using EngineAssetProvider = std::function<bool(AssetManager&)>;
    void registerEngineAssetProvider(EngineAssetProvider provider);

    // 按扩展名找 importer（大小写不敏感）。没有认领者时返回 nullptr。
    const AssetImporter* importerFor(const std::filesystem::path& source_path) const;
    bool canImport(const std::filesystem::path& source_path) const;

    AssetImportResult import(const std::filesystem::path& source_path,
            const AssetImporter& importer);
    // 按扩展名自行选 importer。
    AssetImportResult import(const std::filesystem::path& source_path);

    // 三方对账：Assets/ 里的源文件、磁盘上的 .meta、内存 catalog。
    // planReconcile() 纯读，不改任何状态；applyReconcile() 是唯一的写入点。
    ReconcilePlan planReconcile() const;
    ReconcileReport applyReconcile(const ReconcilePlan& plan);
    // scan + apply 的便捷组合。
    ReconcileReport reconcile();

    AssetIntegrityReport checkIntegrity() const;

    std::shared_ptr<Asset> load(core::UUID handle);

    template<typename T>
    std::shared_ptr<T> load(core::UUID handle) {
        return std::dynamic_pointer_cast<T>(load(handle));
    }

    template<typename T>
    std::shared_ptr<T> load(AssetHandle<T> handle) {
        return std::dynamic_pointer_cast<T>(load(handle.id()));
    }

    std::shared_ptr<Asset> getAsset(core::UUID handle) const noexcept;

    void unload(core::UUID handle) noexcept { m_loaded.erase(handle); }
    void unloadWithDependents(core::UUID handle) noexcept;

    AssetStorage& storage() noexcept { return m_storage; }
    const AssetStorage& storage() const noexcept { return m_storage; }
    AssetCatalog& catalog() noexcept { return m_catalog; }
    const AssetCatalog& catalog() const noexcept { return m_catalog; }

private:
    struct ImporterEntry {
        std::unique_ptr<AssetImporter> importer;
        std::vector<std::string> extensions;
    };

    bool commitOutputs(const std::filesystem::path& normalized_source,
            const AssetImporter& importer, const AssetSettings& stored,
            const ResolvedSettings& resolved, std::vector<AssetImportOutput>& outputs);
    // plan 阶段：跑 prescan 收集跨源引用与设置推断，裁决冲突。
    void collectInferences(ReconcilePlan& plan,
            const std::unordered_set<std::string>& real_files) const;
    // plan 阶段：按 references 做拓扑排序，被引用者排前面。
    void orderByDependencies(ReconcilePlan& plan) const;
    // apply 阶段：把裁决出的推断写进 sidecar，供 import() 读取。
    bool persistInferences(const ReconcileItem& item);
    // 把磁盘 .meta 灌进 catalog 的 User 集合（Engine 条目保持不动）。
    void rebuildUserCatalog(const std::vector<SourceMetadata>& metadata);
    bool importOne(const std::filesystem::path& source_path, ReconcileReport& report);

    AssetStorage m_storage;
    AssetCatalog m_catalog;
    std::vector<ImporterEntry> m_importers;
    std::unordered_map<std::string, AssetImporter*> m_importer_by_extension;
    std::vector<EngineAssetProvider> m_engine_providers;
    std::unordered_map<AssetType, std::unique_ptr<AssetLoader>> m_loaders;
    std::unordered_map<core::UUID, std::weak_ptr<Asset>> m_loaded;
    std::unordered_set<core::UUID> m_loading;
};

}
