#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace arti::asset {

class AssetCatalog;
class AssetStorage;

struct AssetImportOutput {
    AssetMetadata metadata;
    std::vector<std::byte> encoded{};
    std::string source_suffix{};
    bool already_imported{ false };
};

struct AssetImportResult {
    std::vector<AssetImportOutput> outputs;
    std::string error{};

    explicit operator bool() const noexcept { return error.empty(); }
};

class AssetImporter {
public:
    virtual ~AssetImporter() = default;

    virtual std::vector<std::string> getSupportedExtensions() const = 0;

    virtual AssetImportResult import(const std::filesystem::path& source_path) = 0;

    void setWorkspace(AssetStorage& storage, AssetCatalog& catalog) noexcept {
        m_storage = &storage;
        m_catalog = &catalog;
    }

protected:
    bool hasCurrentFiles(const AssetMetadata& metadata) const;

    AssetStorage* m_storage{ nullptr };
    AssetCatalog* m_catalog{ nullptr };

private:
    virtual std::vector<std::byte> encode(const AssetMetadata& metadata,
            const std::filesystem::path& source_path) const = 0;
};

}
