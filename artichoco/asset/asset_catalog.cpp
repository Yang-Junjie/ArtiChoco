#include "asset_catalog.h"

namespace arti::asset {

std::optional<AssetMetadata> AssetCatalog::find(core::UUID handle) const {
    const auto found = m_metadata.find(handle);
    return found == m_metadata.end() ? std::nullopt
                                     : std::optional<AssetMetadata>{ found->second };
}

std::vector<AssetMetadata> AssetCatalog::findBySourcePath(
        const std::filesystem::path& source_path) const {
    std::vector<AssetMetadata> matches;
    if (!isSafeAssetRelativePath(source_path)) {
        return matches;
    }
    const std::filesystem::path normalized = source_path.lexically_normal();
    for (const auto& [handle, metadata] : m_metadata) {
        if (metadata.source_path.lexically_normal() == normalized) {
            matches.push_back(metadata);
        }
    }
    return matches;
}

std::optional<AssetMetadata> AssetCatalog::findBySourcePathAndType(
        const std::filesystem::path& source_path, const AssetType& type) const {
    for (const AssetMetadata& metadata : findBySourcePath(source_path)) {
        if (metadata.type == type) {
            return metadata;
        }
    }
    return std::nullopt;
}

std::vector<AssetMetadata> AssetCatalog::allMetadata() const {
    std::vector<AssetMetadata> metadata;
    metadata.reserve(m_metadata.size());
    for (const auto& [handle, entry] : m_metadata) {
        metadata.push_back(entry);
    }
    return metadata;
}

std::vector<core::UUID> AssetCatalog::dependentsOf(core::UUID handle) const {
    const auto found = m_dependents.find(handle);
    return found == m_dependents.end() ? std::vector<core::UUID>{}
                                       : found->second;
}

void AssetCatalog::insert(AssetMetadata metadata) {
    const auto existing = m_metadata.find(metadata.handle);
    if (existing != m_metadata.end()) {
        for (core::UUID dependency : existing->second.dependencies) {
            const auto found = m_dependents.find(dependency);
            if (found == m_dependents.end()) {
                continue;
            }
            std::erase(found->second, metadata.handle);
            if (found->second.empty()) {
                m_dependents.erase(found);
            }
        }
    }
    for (core::UUID dependency : metadata.dependencies) {
        m_dependents[dependency].push_back(metadata.handle);
    }
    m_metadata.insert_or_assign(metadata.handle, std::move(metadata));
}

void AssetCatalog::clear() noexcept {
    m_metadata.clear();
    m_dependents.clear();
}

} // namespace arti::asset
