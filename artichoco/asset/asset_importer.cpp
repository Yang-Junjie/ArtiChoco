#include "asset_importer.h"

#include "asset_storage.h"

#include <system_error>

namespace arti::asset {

bool AssetImporter::hasCurrentFiles(const AssetMetadata& metadata) const {
    if (m_storage == nullptr) {
        return false;
    }

    std::filesystem::path meta_file = metadata.source_path;
    meta_file += kAssetMetadataExtension;
    const auto meta = m_storage->resolveSourcePath(meta_file);
    const auto artifact = m_storage->resolveArtifactPath(metadata.artifact_path);

    std::error_code error;
    return meta && artifact && std::filesystem::is_regular_file(*meta, error) && !error &&
            std::filesystem::is_regular_file(*artifact, error) && !error;
}

} // namespace arti::asset
