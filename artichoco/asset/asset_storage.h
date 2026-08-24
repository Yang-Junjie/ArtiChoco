#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace arti::asset {

class AssetStorage {
public:
    bool open(std::filesystem::path assets_root, std::filesystem::path artifacts_root);
    void close() noexcept;

    bool isOpen() const noexcept { return !m_assets_root.empty(); }

    std::optional<std::vector<AssetMetadata>> scanMetadata() const;

    bool writeMetadata(const AssetMetadata& metadata);

    bool writeArtifact(const std::filesystem::path& relative_path,
            const std::vector<std::byte>& data);

    std::optional<std::filesystem::path> resolveSourcePath(
            const std::filesystem::path& relative_path) const;
    std::optional<std::filesystem::path> resolveArtifactPath(
            const std::filesystem::path& relative_path) const;

private:
    std::filesystem::path m_assets_root;
    std::filesystem::path m_artifacts_root;
};

}
