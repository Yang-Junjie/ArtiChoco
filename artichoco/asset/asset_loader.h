#pragma once

#include "asset_metadata.h"

#include <filesystem>
#include <memory>
#include <span>

namespace arti::asset {

class AssetManager;

// AssetLoader decodes an artifact into an Asset instance. It is stateless:
// the AssetManager resolves the artifact, loads metadata.dependencies
// recursively, and passes them to decode() in the same order. A loader may
// keep some or all of them alive by storing the shared_ptrs.
class AssetLoader {
public:
    virtual ~AssetLoader() = default;

    [[nodiscard]] virtual AssetType getType() const = 0;

private:
    friend class AssetManager;

    [[nodiscard]] virtual std::shared_ptr<Asset> decode(const AssetMetadata& metadata,
            const std::filesystem::path& artifact_file,
            std::span<const std::shared_ptr<Asset>> dependencies) = 0;
};

} // namespace arti::asset
