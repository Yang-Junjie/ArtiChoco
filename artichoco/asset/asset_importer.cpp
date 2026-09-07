#include "asset_importer.h"

#include "asset_storage.h"

#include <system_error>

namespace arti::asset {

bool AssetImporter::hasCurrentFiles(const AssetMetadata& metadata) const {
    if (m_storage == nullptr) {
        return false;
    }

    // 这里只检查文件和 artifact 是否存在。源内容和 importer 版本的变更由
    // AssetManager::planReconcile() 在导入前统一判断。
    return m_storage->hasSource(AssetStorage::metadataPathFor(metadata.source_path)) &&
            m_storage->hasArtifact(metadata.artifact_path);
}

}
