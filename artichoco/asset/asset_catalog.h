#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arti::asset {

// 资产条目的来源。决定它是否参与 reconcile。
//  - User:   由 Assets/ 下的源文件导入而来，身份持久化在 .meta sidecar 里
//  - Engine: 引擎自带，身份是编译期常量，磁盘上没有源文件也没有 .meta
enum class AssetOrigin : uint8_t { User, Engine };

struct AssetEntry final {
    AssetMetadata metadata;
    AssetOrigin origin{ AssetOrigin::User };
};

// insert() 的结果。Conflicted 表示这个 handle 已经被另一个 (source_path, type) 占用，
// 插入被拒绝 —— 先到者胜，保证结果与插入顺序无关的那一半（谁赢由调用方的遍历序决定，
// 但"不会静默覆盖"是确定的）。
enum class AssetInsertStatus : uint8_t { Added, Updated, Conflicted };

// 内存中已导入资产的目录。绝不接触文件系统。
//
// 索引：handle → entry（主键）、source_path → handles（精确匹配）、
// dependency → dependents（依赖反向图）。
// revision() 在任何变更后自增，供 UI 判断缓存失效。
class AssetCatalog {
public:
    std::optional<AssetMetadata> find(core::UUID handle) const;
    const AssetEntry* findEntry(core::UUID handle) const;
    AssetOrigin originOf(core::UUID handle) const;

    // 一个源文件产出的全部资产（含子资产）。一源一 sidecar 之后这就是完整分组，
    // 不再需要按 suffix 回溯归属。
    std::vector<AssetMetadata> findBySourcePath(const std::filesystem::path& source_path) const;
    // 同上，但只返回 handle。用于在重导入后剔除不再产出的条目。
    std::vector<core::UUID> handlesForSource(const std::filesystem::path& source_path) const;
    // identity 查询：(source_path, local_id)。importer 靠它复用既有 handle。
    std::optional<AssetMetadata> findBySourceAndLocalId(const std::filesystem::path& source_path,
            std::string_view local_id) const;
    std::optional<AssetMetadata> findBySourcePathAndType(const std::filesystem::path& source_path,
            const AssetType& type) const;

    std::vector<AssetMetadata> allMetadata() const;
    std::vector<AssetEntry> allEntries() const;
    std::vector<AssetEntry> entriesWithOrigin(AssetOrigin origin) const;

    std::vector<core::UUID> dependentsOf(core::UUID handle) const;

    AssetInsertStatus insert(AssetMetadata metadata, AssetOrigin origin = AssetOrigin::User);
    bool erase(core::UUID handle);
    // 移除指定来源的全部条目，其他来源保持不动。reconcile 用它重建 User 集合。
    size_t eraseOrigin(AssetOrigin origin);
    void clear() noexcept;

    size_t importedCount() const noexcept { return m_entries.size(); }
    uint64_t revision() const noexcept { return m_revision; }

private:
    void addIndices(const AssetMetadata& metadata);
    void removeIndices(const AssetMetadata& metadata);

    static std::string sourceKey(const std::filesystem::path& source_path);

    std::unordered_map<core::UUID, AssetEntry> m_entries;
    std::unordered_map<std::string, std::vector<core::UUID>> m_by_source;
    std::unordered_map<core::UUID, std::vector<core::UUID>> m_dependents;
    uint64_t m_revision{ 0 };
};

}
