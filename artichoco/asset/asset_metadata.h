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

inline constexpr std::string_view kAssetMetadataExtension{ ".meta" };

using Value = std::variant<bool, int64_t, uint64_t, double, std::string, std::vector<uint64_t>>;

struct AssetMetadata final {
    core::UUID handle;
    AssetType type;

    std::filesystem::path source_path;
    std::filesystem::path artifact_path;

    std::vector<core::UUID> dependencies;

    std::unordered_map<std::string, Value> properties;
    bool operator==(const AssetMetadata&) const = default;
};

bool isSafeAssetRelativePath(const std::filesystem::path& path,
        bool allow_empty = false);
bool isValidAssetMetadata(const AssetMetadata& metadata);

std::optional<std::string> serializeAssetMetadata(const AssetMetadata& metadata);
std::optional<AssetMetadata> deserializeAssetMetadata(std::string_view text);

}
