#include "asset_importer.h"

#include "asset_storage.h"

#include <system_error>

namespace arti::asset {

bool AssetImporter::hasCurrentFiles(const AssetMetadata& metadata) const {
    if (m_storage == nullptr) {
        return false;
    }

    // 注意：只检查文件是否存在，不检查源文件内容有没有变 —— sidecar 里的
    // ContentHash 目前只写不读（变更检测排在多线程之后）。所以 importer 不能
    // 依赖它来决定"能否跳过重新编码"，除非源变更检测做完。
    return m_storage->hasSource(AssetStorage::metadataPathFor(metadata.source_path)) &&
            m_storage->hasArtifact(metadata.artifact_path);
}

}
