#pragma once
#include "artichoco/asset/asset.h"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace arti::asset {

// The metadata sidecar extension shared by every Asset, regardless of the
// importer that produced it. Sub-assets append "#name" to the source path,
// so their sidecar files are distinguished by the source file name itself.
inline constexpr std::string_view kAssetMetadataExtension{ ".meta" };

using Value = std::variant<bool, int64_t, uint64_t, double, std::string, std::vector<uint64_t>>;

struct AssetMetadata final {
    core::UUID handle;
    AssetType type;

    // relative to the .artiproj file
    std::filesystem::path source_path;
    std::filesystem::path artifact_path;

    // Assets this Asset references; used for dependency tracking and
    // reimport invalidation. Handles are stable, so changes to a
    // dependency's contents do not require a reimport of the dependent.
    std::vector<core::UUID> dependencies;

    std::unordered_map<std::string, Value> properties;
    bool operator==(const AssetMetadata&) const = default;
};

[[nodiscard]] bool isSafeAssetRelativePath(const std::filesystem::path& path,
        bool allow_empty = false);
[[nodiscard]] bool isValidAssetMetadata(const AssetMetadata& metadata);

[[nodiscard]] std::optional<std::string> serializeAssetMetadata(const AssetMetadata& metadata);
[[nodiscard]] std::optional<AssetMetadata> deserializeAssetMetadata(std::string_view text);

} // namespace arti::asset