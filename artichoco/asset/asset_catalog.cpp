#include "asset_catalog.h"

#include "asset_log.h"

#include <algorithm>
#include <utility>

namespace arti::asset {

std::string AssetCatalog::sourceKey(const std::filesystem::path& source_path) {
    return source_path.lexically_normal().generic_string();
}

std::optional<AssetMetadata> AssetCatalog::find(core::UUID handle) const {
    const auto found = m_entries.find(handle);
    return found == m_entries.end() ? std::nullopt
                                    : std::optional<AssetMetadata>{ found->second.metadata };
}

const AssetEntry* AssetCatalog::findEntry(core::UUID handle) const {
    const auto found = m_entries.find(handle);
    return found == m_entries.end() ? nullptr : &found->second;
}

AssetOrigin AssetCatalog::originOf(core::UUID handle) const {
    const auto found = m_entries.find(handle);
    return found == m_entries.end() ? AssetOrigin::User : found->second.origin;
}

std::vector<AssetMetadata> AssetCatalog::findBySourcePath(
        const std::filesystem::path& source_path) const {
    std::vector<AssetMetadata> matches;
    if (!isSafeAssetRelativePath(source_path)) {
        return matches;
    }
    const auto found = m_by_source.find(sourceKey(source_path));
    if (found == m_by_source.end()) {
        return matches;
    }
    matches.reserve(found->second.size());
    for (const core::UUID handle: found->second) {
        if (const auto entry = m_entries.find(handle); entry != m_entries.end()) {
            matches.push_back(entry->second.metadata);
        }
    }
    return matches;
}

std::vector<core::UUID> AssetCatalog::handlesForSource(
        const std::filesystem::path& source_path) const {
    if (!isSafeAssetRelativePath(source_path)) {
        return {};
    }
    const auto found = m_by_source.find(sourceKey(source_path));
    return found == m_by_source.end() ? std::vector<core::UUID>{} : found->second;
}

std::optional<AssetMetadata> AssetCatalog::findBySourceAndLocalId(
        const std::filesystem::path& source_path, std::string_view local_id) const {
    if (!isSafeAssetRelativePath(source_path)) {
        return std::nullopt;
    }
    const auto found = m_by_source.find(sourceKey(source_path));
    if (found == m_by_source.end()) {
        return std::nullopt;
    }
    for (const core::UUID handle: found->second) {
        const auto entry = m_entries.find(handle);
        if (entry != m_entries.end() && entry->second.metadata.local_id == local_id) {
            return entry->second.metadata;
        }
    }
    return std::nullopt;
}

std::optional<AssetMetadata> AssetCatalog::findBySourcePathAndType(
        const std::filesystem::path& source_path, const AssetType& type) const {
    if (!isSafeAssetRelativePath(source_path)) {
        return std::nullopt;
    }
    const auto found = m_by_source.find(sourceKey(source_path));
    if (found == m_by_source.end()) {
        return std::nullopt;
    }
    for (const core::UUID handle: found->second) {
        const auto entry = m_entries.find(handle);
        if (entry != m_entries.end() && entry->second.metadata.type == type) {
            return entry->second.metadata;
        }
    }
    return std::nullopt;
}

std::vector<AssetMetadata> AssetCatalog::allMetadata() const {
    std::vector<AssetMetadata> metadata;
    metadata.reserve(m_entries.size());
    for (const auto& [handle, entry]: m_entries) {
        metadata.push_back(entry.metadata);
    }
    return metadata;
}

std::vector<AssetEntry> AssetCatalog::allEntries() const {
    std::vector<AssetEntry> entries;
    entries.reserve(m_entries.size());
    for (const auto& [handle, entry]: m_entries) {
        entries.push_back(entry);
    }
    return entries;
}

std::vector<AssetEntry> AssetCatalog::entriesWithOrigin(AssetOrigin origin) const {
    std::vector<AssetEntry> entries;
    for (const auto& [handle, entry]: m_entries) {
        if (entry.origin == origin) {
            entries.push_back(entry);
        }
    }
    return entries;
}

std::vector<core::UUID> AssetCatalog::dependentsOf(core::UUID handle) const {
    const auto found = m_dependents.find(handle);
    return found == m_dependents.end() ? std::vector<core::UUID>{} : found->second;
}

void AssetCatalog::addIndices(const AssetMetadata& metadata) {
    m_by_source[sourceKey(metadata.source_path)].push_back(metadata.handle);
    for (const core::UUID dependency: metadata.dependencies) {
        m_dependents[dependency].push_back(metadata.handle);
    }
}

void AssetCatalog::removeIndices(const AssetMetadata& metadata) {
    if (const auto found = m_by_source.find(sourceKey(metadata.source_path));
            found != m_by_source.end()) {
        std::erase(found->second, metadata.handle);
        if (found->second.empty()) {
            m_by_source.erase(found);
        }
    }
    for (const core::UUID dependency: metadata.dependencies) {
        if (const auto found = m_dependents.find(dependency); found != m_dependents.end()) {
            std::erase(found->second, metadata.handle);
            if (found->second.empty()) {
                m_dependents.erase(found);
            }
        }
    }
}

AssetInsertStatus AssetCatalog::insert(AssetMetadata metadata, AssetOrigin origin) {
    const auto existing = m_entries.find(metadata.handle);
    if (existing == m_entries.end()) {
        addIndices(metadata);
        m_entries.emplace(metadata.handle, AssetEntry{ std::move(metadata), origin });
        ++m_revision;
        return AssetInsertStatus::Added;
    }

    const AssetMetadata& previous = existing->second.metadata;
    const bool same_identity =
            previous.source_path.lexically_normal() == metadata.source_path.lexically_normal() &&
            previous.local_id == metadata.local_id;
    if (!same_identity) {
        // 同一个 UUID 声称两个不同身份 —— 通常是用户拷贝了带 .meta 的目录。
        // 静默覆盖会丢资产，所以拒绝后来者并报告。
        getLogChannel().error(
                "Asset UUID conflict on {}: '{}' [{}] is already registered, rejecting '{}' [{}]",
                metadata.handle.toString(), previous.source_path.generic_string(),
                previous.local_id, metadata.source_path.generic_string(), metadata.local_id);
        return AssetInsertStatus::Conflicted;
    }

    removeIndices(previous);
    addIndices(metadata);
    existing->second = AssetEntry{ std::move(metadata), origin };
    ++m_revision;
    return AssetInsertStatus::Updated;
}

bool AssetCatalog::erase(core::UUID handle) {
    const auto found = m_entries.find(handle);
    if (found == m_entries.end()) {
        return false;
    }
    removeIndices(found->second.metadata);
    m_entries.erase(found);
    ++m_revision;
    return true;
}

size_t AssetCatalog::eraseOrigin(AssetOrigin origin) {
    std::vector<core::UUID> doomed;
    for (const auto& [handle, entry]: m_entries) {
        if (entry.origin == origin) {
            doomed.push_back(handle);
        }
    }
    for (const core::UUID handle: doomed) {
        const auto found = m_entries.find(handle);
        removeIndices(found->second.metadata);
        m_entries.erase(found);
    }
    if (!doomed.empty()) {
        ++m_revision;
    }
    return doomed.size();
}

void AssetCatalog::clear() noexcept {
    m_entries.clear();
    m_by_source.clear();
    m_dependents.clear();
    ++m_revision;
}

}
