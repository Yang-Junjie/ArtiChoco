#pragma once
#include "artichoco/asset/asset.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace arti::asset {

inline constexpr std::string_view kAssetMetadataExtension{ ".meta" };

// sidecar 格式版本。不匹配时保留 Settings.Authored、其余重新导入。
inline constexpr uint32_t kSourceMetadataVersion{ 2 };

using Value = std::variant<bool, int64_t, uint64_t, double, std::string, std::vector<uint64_t>>;

using SettingMap = std::unordered_map<std::string, Value>;

// 一次推断的出处。同一个键被多个容器推断时，冲突由 planner 裁决。
struct InferredSetting final {
    Value value;
    std::filesystem::path by;   // 发布这条推断的源文件
    std::string usage;          // "normal_texture"，给 UI 解释理由
    bool operator==(const InferredSetting&) const = default;
};

// 导入设置。Authored 里"键的存在"本身是信息 —— 缺失表示未指定、往下层取，
// 不等于 false。所以解析后的有效值绝不能写回 Authored。
struct AssetSettings final {
    SettingMap authored;
    std::unordered_map<std::string, InferredSetting> inferred;
    bool operator==(const AssetSettings&) const = default;
};

// 源文件指纹。当前只写不读 —— 变更检测排在多线程之后，先把槽位留出来，
// 避免那时再做一次格式变更。
struct SourceFingerprint final {
    uint64_t content_hash{ 0 };
    uint64_t size{ 0 };
    bool operator==(const SourceFingerprint&) const = default;
};

struct ImporterStamp final {
    std::string name;
    uint32_t version{ 0 };
    bool operator==(const ImporterStamp&) const = default;
};

// 一个源文件产出的单个资产。identity 是 (source_path, local_id)：
// local_id 为空表示"源文件本身就是唯一产出"，非空表示子资产。
// 它不再拼进文件名，所以没有路径安全约束，也不需要 suffix 回溯。
struct AssetRecord final {
    core::UUID handle;
    AssetType type;
    std::string local_id;

    std::filesystem::path artifact_path;
    std::vector<core::UUID> dependencies;
    std::unordered_map<std::string, Value> properties;

    bool operator==(const AssetRecord&) const = default;
};

// 一个源文件一份 sidecar：<source>.meta。
struct SourceMetadata final {
    uint32_t version{ kSourceMetadataVersion };
    std::filesystem::path source_path;
    SourceFingerprint fingerprint;
    ImporterStamp importer;
    AssetSettings settings;
    // 解析后有效设置的哈希（不是 Authored 的哈希）—— 这样在代码里改一个默认值
    // 也能让依赖它的资产失效。目前只写不读，与 fingerprint 一起等变更检测。
    uint64_t settings_hash{ 0 };
    std::vector<AssetRecord> assets;

    bool operator==(const SourceMetadata&) const = default;
};

// catalog 里的展开形式：一条 AssetRecord 加上它所属的源文件。
struct AssetMetadata final {
    core::UUID handle;
    AssetType type;
    std::string local_id;

    std::filesystem::path source_path;
    std::filesystem::path artifact_path;

    std::vector<core::UUID> dependencies;
    std::unordered_map<std::string, Value> properties;

    bool operator==(const AssetMetadata&) const = default;
};

bool isSafeAssetRelativePath(const std::filesystem::path& path, bool allow_empty = false);
bool isValidLocalId(std::string_view local_id);
bool isValidAssetRecord(const AssetRecord& record);
bool isValidSourceMetadata(const SourceMetadata& metadata);
bool isValidAssetMetadata(const AssetMetadata& metadata);

// SourceMetadata ↔ 展开的 AssetMetadata 列表。
std::vector<AssetMetadata> expandSourceMetadata(const SourceMetadata& metadata);

std::optional<std::string> serializeSourceMetadata(const SourceMetadata& metadata);
std::optional<SourceMetadata> deserializeSourceMetadata(std::string_view text);

}
