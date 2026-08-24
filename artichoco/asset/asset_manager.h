#pragma once

#include "asset_catalog.h"
#include "asset_importer.h"
#include "asset_loader.h"
#include "asset_storage.h"

#include <filesystem>
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

    std::vector<AssetImportResult> import(const std::filesystem::path& source_path,
            const AssetImporter& importer);

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

    void importOutputs(std::vector<AssetImportOutput>& outputs,
            const std::filesystem::path& normalized_source);

    AssetStorage m_storage;
    AssetCatalog m_catalog;
    std::vector<ImporterEntry> m_importers;
    std::unordered_map<AssetType, std::unique_ptr<AssetLoader>> m_loaders;
    std::unordered_map<core::UUID, std::weak_ptr<Asset>> m_loaded;
    std::unordered_set<core::UUID> m_loading;
};

}
