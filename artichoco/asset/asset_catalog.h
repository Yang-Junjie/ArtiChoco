#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

namespace arti::asset {

class AssetCatalog {
public:
    std::optional<AssetMetadata> find(core::UUID handle) const;
    std::vector<AssetMetadata> findBySourcePath(
            const std::filesystem::path& source_path) const;
    std::optional<AssetMetadata> findBySourcePathAndType(
            const std::filesystem::path& source_path, const AssetType& type) const;
    std::vector<AssetMetadata> allMetadata() const;

    std::vector<core::UUID> dependentsOf(core::UUID handle) const;

    void insert(AssetMetadata metadata);
    void clear() noexcept;

    size_t importedCount() const noexcept { return m_metadata.size(); }

private:
    std::unordered_map<core::UUID, AssetMetadata> m_metadata;
    std::unordered_map<core::UUID, std::vector<core::UUID>> m_dependents;
};

}
