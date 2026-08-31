#pragma once

#include "asset_metadata.h"
#include "asset_storage.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace arti::asset {

// 一个源文件在本轮 reconcile 里该被怎么处理。
enum class ReconcileAction : uint8_t {
    Import,     // 有源文件、没有任何 .meta
    Reimport,   // 有源文件也有 .meta，但 artifact 缺失（Library 被删/损坏）
    Current,    // 有源文件、.meta 与 artifact 都齐全，无需动作
    Unsupported // 有源文件，但没有 importer 认领这个扩展名
};

struct ReconcileItem final {
    std::filesystem::path source_path;
    ReconcileAction action{ ReconcileAction::Current };
    // 该源文件当前已登记的资产（含子资产）。Import 时为空。
    std::vector<AssetMetadata> existing;
    // 触发 Reimport 的原因，供日志和 UI 显示。
    std::string reason;
    // 由容器推断出来的设置（已裁决冲突）。apply() 在导入前写进 sidecar，
    // 这样 TextureImporter 就能拿到"这张图是法线贴图"这类它自己无从判断的信息。
    std::unordered_map<std::string, InferredSetting> inferred;
    // 本源文件引用了哪些别的源文件（由 prescan 声明）。拓扑排序用。
    std::vector<std::filesystem::path> references;
};

// 同一个键被多个容器推断出不同值。一个源文件只有一份设置（Godot 模型），
// 所以只能有一个胜出 —— 按发布者 source_path 字典序裁决，其余记在这里。
// 这是"一文件一设置"的固有代价，不静默。
struct InferenceConflict final {
    std::filesystem::path target_source;
    std::string key;
    std::filesystem::path winner;
    std::filesystem::path loser;
    std::string winner_usage;
    std::string loser_usage;
};

// 有 .meta 但源文件已经不在了 —— 通常是用户在文件管理器里删掉了源文件。
// .meta 和 artifact 都是派生数据，apply() 会一起清掉。
struct ReconcileOrphan final {
    AssetMetadata metadata;
    // sidecar 声称的源文件。一源一 sidecar，所以这就是 metadata.source_path。
    std::filesystem::path owning_source;
};

// 同一个 UUID 被多份 .meta 声称。第一个胜出，其余列在这里。
struct ReconcileConflict final {
    core::UUID handle;
    std::filesystem::path kept_source;
    std::filesystem::path rejected_source;
};

// scan 阶段的产物：只读磁盘、不改任何状态，可以直接喂给 UI 当视图。
struct ReconcilePlan final {
    // 磁盘上被接受的 sidecar（已剔除 UUID 冲突条目和孤儿），
    // apply() 用它重建 catalog 的 User 集合。
    std::vector<SourceMetadata> accepted_metadata;
    // 按 source_path 字典序排列，保证 apply 顺序确定。
    std::vector<ReconcileItem> items;
    std::vector<ReconcileOrphan> orphans;
    std::vector<ReconcileConflict> conflicts;
    std::vector<InferenceConflict> inference_conflicts;
    std::vector<MetadataIssue> metadata_issues;
    // 依赖成环的源文件（A 引用 B、B 引用 A）。按字典序破环后照常导入。
    std::vector<std::filesystem::path> dependency_cycles;
    std::string traversal_error;

    size_t countWithAction(ReconcileAction action) const;
    bool hasWork() const;
    bool complete() const noexcept { return traversal_error.empty(); }
};

struct ReconcileReport final {
    size_t imported{ 0 };
    size_t reimported{ 0 };
    size_t current{ 0 };
    size_t unsupported{ 0 };
    size_t failed{ 0 };
    size_t forgotten{ 0 };        // 从 catalog 移除的孤儿条目数
    size_t removed_metadata{ 0 }; // 删掉的 .meta 文件数
    size_t removed_artifacts{ 0 };
    std::vector<std::string> errors;

    bool succeeded() const noexcept { return failed == 0 && errors.empty(); }
};

// 校验（只读）：报告 artifact 缺失和依赖悬空。
struct AssetIntegrityIssue final {
    core::UUID handle;
    std::string message;
};

struct AssetIntegrityReport final {
    size_t assets_checked{ 0 };
    std::vector<AssetIntegrityIssue> issues;

    bool succeeded() const noexcept { return issues.empty(); }
};

}
