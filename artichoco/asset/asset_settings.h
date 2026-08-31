#pragma once

#include "asset_metadata.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arti::asset {

// 某个键的有效值来自哪一层。给 Inspector 显示"这个值是谁定的"。
enum class SettingLayer : uint8_t {
    Default,   // importer 声明的默认值
    Inferred,  // 由引用它的容器推断（如 glTF 说"这是法线贴图"）
    Authored,  // 用户显式设定，优先级最高
};

// importer 对一个设置键的声明。schema 是权威 —— 解析后每个 schema 键都保证
// 存在且类型正确，所以 importer 里不需要写 fallback 分支。
struct SettingDescriptor final {
    std::string key;
    Value default_value;
    // 字符串枚举型的合法取值。为空表示不限制。
    std::vector<std::string> allowed;
    // 数值型区间。
    std::optional<double> min;
    std::optional<double> max;
    std::string doc;

    // 前缀族：key 是前缀而不是完整键名，匹配任意数量的实际键。
    // 用于键集在编译期未知的设置 —— 比如 Extract 的
    // "ExtractedMaterial.<local_id>"，local_id 只有导入时才知道。
    // 前缀族没有默认值（没有匹配就是空），用 withPrefix() 枚举。
    bool is_prefix{ false };
};

enum class SettingIssueKind : uint8_t {
    InvalidAuthored,  // 用户值不合 schema，回退默认
    InvalidInferred,  // 推断值不合 schema，回退默认
    UnknownKey,       // Authored 里有 schema 不认识的键（保留不删）
};

struct SettingIssue final {
    std::string key;
    SettingIssueKind kind{ SettingIssueKind::InvalidAuthored };
    std::string detail;
};

// 解析后的有效设置。取值器不可失败：schema 里的键一定存在且类型正确，
// 取一个不在 schema 里的键是编程错误。
class ResolvedSettings final {
public:
    ResolvedSettings() = default;

    bool getBool(std::string_view key) const;
    int64_t getInt(std::string_view key) const;
    double getDouble(std::string_view key) const;
    const std::string& getString(std::string_view key) const;

    bool has(std::string_view key) const;
    SettingLayer layerOf(std::string_view key) const;
    // 枚举某个前缀族下的全部键值对，按键名排序（与遍历序无关）。
    std::vector<std::pair<std::string, Value>> withPrefix(std::string_view prefix) const;
    const std::vector<SettingIssue>& issues() const noexcept { return m_issues; }
    const SettingMap& values() const noexcept { return m_values; }

    // 规范化后的哈希，用于判断"设置有没有变"。哈希的是 resolved 而不是
    // authored —— 这样在代码里改一个默认值也能触发重导。
    uint64_t hash() const;

private:
    friend ResolvedSettings resolveSettings(const std::vector<SettingDescriptor>& schema,
            const AssetSettings& settings);

    SettingMap m_values;
    std::unordered_map<std::string, SettingLayer> m_layers;
    std::vector<SettingIssue> m_issues;
};

// 逐键解析：default → inferred → authored。逐键而不是整块覆盖 —— 用户改了
// Colorspace 不该把容器对 Filter 的推断一起废掉。
ResolvedSettings resolveSettings(const std::vector<SettingDescriptor>& schema,
        const AssetSettings& settings);

bool settingMatchesDescriptor(const SettingDescriptor& descriptor, const Value& value);

}
