#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace arti::asset {

// AssetStorage owns the workspace layout and all file I/O: the Assets root
// (sources and their .meta sidecars) and the Artifacts root (generated
// artifacts). Every path crossing this class is relative to the project.
class AssetStorage {
public:
    [[nodiscard]] bool open(std::filesystem::path assets_root,
            std::filesystem::path artifacts_root);
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return !m_assets_root.empty(); }

    // Reads and validates every .meta sidecar under the Assets root.
    // Returns nullopt when a sidecar cannot be read or is misplaced.
    [[nodiscard]] std::optional<std::vector<AssetMetadata>> scanMetadata() const;

    // Writes the .meta sidecar next to the source the metadata describes.
    [[nodiscard]] bool writeMetadata(const AssetMetadata& metadata);

    [[nodiscard]] bool writeArtifact(const std::filesystem::path& relative_path,
            const std::vector<std::byte>& data);

    [[nodiscard]] std::optional<std::filesystem::path> resolveSourcePath(
            const std::filesystem::path& relative_path) const;
    [[nodiscard]] std::optional<std::filesystem::path> resolveArtifactPath(
            const std::filesystem::path& relative_path) const;

private:
    std::filesystem::path m_assets_root;
    std::filesystem::path m_artifacts_root;
};

} // namespace arti::asset
