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
    // Sub-asset identity appended to the source path (e.g. "#mesh_0"), giving
    // each output its own .meta file. Empty keeps the plain source path.
    std::string source_suffix{};
    // Set by an importer that verified the existing .meta and artifact are
    // still current on disk: the output reuses the stored handle and the
    // manager must not write any file.
    bool already_imported{ false };
};

struct AssetImportResult {
    std::vector<AssetImportOutput> outputs;
    std::string error{};

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

class AssetImporter {
public:
    virtual ~AssetImporter() = default;

    [[nodiscard]] virtual std::vector<std::string> getSupportedExtensions() const = 0;

    // source_path is relative to the project's Assets root and identifies the
    // imported source (sub-assets append "#name"). Importers use m_storage to
    // read the file and m_catalog to query the existing identity of an Asset
    // across reimports.
    [[nodiscard]] virtual AssetImportResult import(const std::filesystem::path& source_path) = 0;

    void setWorkspace(AssetStorage& storage, AssetCatalog& catalog) noexcept {
        m_storage = &storage;
        m_catalog = &catalog;
    }

protected:
    // True when the .meta sidecar and the artifact of an existing Asset are
    // still present on disk, i.e. the files the manager would otherwise
    // rewrite. Used together with the identity query to decide
    // AssetImportOutput::already_imported.
    [[nodiscard]] bool hasCurrentFiles(const AssetMetadata& metadata) const;

    AssetStorage* m_storage{ nullptr };
    AssetCatalog* m_catalog{ nullptr };

private:
    [[nodiscard]] virtual std::vector<std::byte> encode(const AssetMetadata& metadata,
            const std::filesystem::path& source_path) const = 0;
};

} // namespace arti::asset
