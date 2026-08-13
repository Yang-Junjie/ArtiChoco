#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

namespace arti::asset {

// AssetCatalog indexes the imported assets in memory: the metadata table
// keyed by handle, lookups by source identity, and the dependency index
// (asset -> assets that reference it). It never touches the file system.
class AssetCatalog {
public:
    [[nodiscard]] std::optional<AssetMetadata> find(core::UUID handle) const;
    [[nodiscard]] std::vector<AssetMetadata> findBySourcePath(
            const std::filesystem::path& source_path) const;
    // The Asset whose identity is (source_path, type): the authoritative query
    // importers use to preserve handles across reimports.
    [[nodiscard]] std::optional<AssetMetadata> findBySourcePathAndType(
            const std::filesystem::path& source_path, const AssetType& type) const;
    [[nodiscard]] std::vector<AssetMetadata> allMetadata() const;

    // Assets whose metadata declares a dependency on the given Asset.
    [[nodiscard]] std::vector<core::UUID> dependentsOf(core::UUID handle) const;

    // Inserts or replaces the metadata of an Asset and maintains the
    // dependency index.
    void insert(AssetMetadata metadata);
    void clear() noexcept;

    [[nodiscard]] size_t importedCount() const noexcept { return m_metadata.size(); }

private:
    std::unordered_map<core::UUID, AssetMetadata> m_metadata;
    std::unordered_map<core::UUID, std::vector<core::UUID>> m_dependents;
};

} // namespace arti::asset
