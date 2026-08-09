#include "asset_metadata.h"

#include <cmath>
#include <unordered_set>

namespace arti::asset {

bool isSafeAssetRelativePath(const std::filesystem::path& path, bool allow_empty) {
    if (path.empty()) {
        return allow_empty;
    }
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }

    const std::filesystem::path normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        return false;
    }
    for (const auto& component: normalized) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool isValidAssetMetadata(const AssetMetadata& metadata) {
    if (!metadata.handle.isValid() || !metadata.type.isValid() ||
            !isSafeAssetRelativePath(metadata.source_path) ||
            !isSafeAssetRelativePath(metadata.artifact_path)) {
        return false;
    }

    std::unordered_set<core::UUID> dependencies;
    dependencies.reserve(metadata.dependencies.size());
    for (core::UUID dependency: metadata.dependencies) {
        if (!dependency.isValid() || dependency == metadata.handle ||
                !dependencies.insert(dependency).second) {
            return false;
        }
    }

    for (const auto& [key, value]: metadata.properties) {
        if (key.empty()) {
            return false;
        }
        if (const auto* number = std::get_if<double>(&value.data());
                number != nullptr && !std::isfinite(*number)) {
            return false;
        }
    }
    return true;
}

} // namespace arti::asset
