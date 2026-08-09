#pragma once

#include "asset_metadata.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace arti::asset {

struct AssetImportContext {
    std::filesystem::path source_path;
    std::filesystem::path source_file;
    std::filesystem::path artifacts_root;
    AssetMetadata metadata;
    bool reimporting{ false };
};

struct AssetImportResult {
    std::optional<AssetMetadata> metadata;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return metadata.has_value(); }
};

class AssetImporter {
public:
    virtual ~AssetImporter() = default;

    [[nodiscard]] virtual std::vector<std::string> getSupportedExtensions() const = 0;
    [[nodiscard]] virtual AssetImportResult import(const AssetImportContext& context) = 0;
};

} // namespace arti::asset
