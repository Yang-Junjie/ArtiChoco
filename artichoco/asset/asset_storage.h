#pragma once

#include "asset_metadata.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace arti::asset {

enum class MetadataIssueKind : uint8_t {
    Unreadable,   // 文件读不出来
    Malformed,    // 解析失败或校验不过
    Misplaced,    // .meta 里的 SourcePath 与它自己的文件名不符
};

struct MetadataIssue final {
    std::filesystem::path meta_file;  // 相对 assets_root
    MetadataIssueKind kind{ MetadataIssueKind::Malformed };
    std::string detail;
};

// scanMetadata() 的结果。坏条目被隔离进 issues，不影响其余条目 ——
// 一个损坏的 .meta 不该让整个项目打不开。
struct MetadataScan final {
    std::vector<SourceMetadata> entries;
    std::vector<MetadataIssue> issues;
    std::string traversal_error;

    bool complete() const noexcept { return traversal_error.empty(); }
};

// 磁盘上的一个源文件。
struct SourceFile final {
    std::filesystem::path relative_path;
    uint64_t size{ 0 };
    int64_t modified_time{ 0 };  // 自 file_clock epoch 起的秒数，取不到则为 0
};

struct SourceScan final {
    std::vector<SourceFile> files;
    std::string traversal_error;

    bool complete() const noexcept { return traversal_error.empty(); }
};

class AssetStorage {
public:
    bool open(std::filesystem::path assets_root, std::filesystem::path artifacts_root);
    void close() noexcept;

    bool isOpen() const noexcept { return !m_assets_root.empty(); }

    // 递归扫描 assets_root 下所有 .meta。
    MetadataScan scanMetadata() const;
    // 递归扫描 assets_root 下所有非 .meta 的常规文件。
    SourceScan scanSources() const;

    bool writeMetadata(const SourceMetadata& metadata);
    std::optional<SourceMetadata> readMetadata(const std::filesystem::path& source_path) const;
    // 删除 source_path 对应的 .meta。文件本来就不在时返回 true。
    bool removeMetadata(const std::filesystem::path& source_path);

    bool writeArtifact(const std::filesystem::path& relative_path,
            const std::vector<std::byte>& data);
    bool removeArtifact(const std::filesystem::path& relative_path);

    bool hasSource(const std::filesystem::path& relative_path) const;
    bool hasArtifact(const std::filesystem::path& relative_path) const;
    std::optional<uint64_t> sourceSize(const std::filesystem::path& relative_path) const;

    std::optional<std::filesystem::path> resolveSourcePath(
            const std::filesystem::path& relative_path) const;
    // resolveSourcePath 的逆运算：绝对路径 → Assets-relative。
    // 落在 assets_root 外面时返回 nullopt。prescan 用它把 glTF 里的外部图片
    // 引用换成资产身份空间里的路径。
    std::optional<std::filesystem::path> relativeSourcePath(
            const std::filesystem::path& absolute_path) const;
    std::optional<std::filesystem::path> resolveArtifactPath(
            const std::filesystem::path& relative_path) const;
    // source_path 对应的 .meta sidecar 路径（相对 assets_root）。
    static std::filesystem::path metadataPathFor(const std::filesystem::path& source_path);

private:
    std::filesystem::path m_assets_root;
    std::filesystem::path m_artifacts_root;
};

}
