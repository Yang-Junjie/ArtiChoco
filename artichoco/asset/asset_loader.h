#pragma once

#include "asset_metadata.h"

#include <filesystem>
#include <memory>

namespace arti::asset {

struct AssetLoadContext {
    const AssetMetadata& metadata;
    std::filesystem::path artifact_file;
};

class AssetLoader {
public:
    virtual ~AssetLoader() = default;

    [[nodiscard]] virtual AssetType getType() const = 0;
    [[nodiscard]] virtual std::shared_ptr<Asset> load(const AssetLoadContext& context) = 0;
};

} // namespace arti::asset
