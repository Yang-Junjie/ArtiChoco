#pragma once

#include "asset_metadata.h"

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace arti::asset {

class AssetManager;

class AssetLoader {
public:
    virtual ~AssetLoader() = default;

    virtual AssetType getType() const = 0;

private:
    friend class AssetManager;

    virtual std::shared_ptr<Asset> decode(const AssetMetadata& metadata,
            const std::filesystem::path& artifact_file,
            std::span<const std::shared_ptr<Asset>> dependencies) = 0;
};

}
