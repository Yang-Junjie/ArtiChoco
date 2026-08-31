#include "asset_settings.h"

#include "asset_log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>
#include <stdexcept>

namespace arti::asset {
namespace {

// FNV-1a 64。跨运行跨平台稳定 —— std::hash 不保证这一点，写进文件就是错的。
constexpr uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr uint64_t kFnvPrime = 0x100000001B3ULL;

uint64_t fnv1a(uint64_t seed, const void* data, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    uint64_t hash = seed;
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t hashString(uint64_t seed, std::string_view text) {
    return fnv1a(seed, text.data(), text.size());
}

// 类型 tag 参与哈希，避免 int64(1) 和 uint64(1) 撞到一起。
uint64_t hashValue(uint64_t seed, const Value& value) {
    uint64_t hash = seed;
    if (const auto* v = std::get_if<bool>(&value)) {
        hash = hashString(hash, "b");
        const unsigned char byte = *v ? 1 : 0;
        return fnv1a(hash, &byte, 1);
    }
    if (const auto* v = std::get_if<int64_t>(&value)) {
        hash = hashString(hash, "i");
        return fnv1a(hash, v, sizeof(*v));
    }
    if (const auto* v = std::get_if<uint64_t>(&value)) {
        hash = hashString(hash, "u");
        return fnv1a(hash, v, sizeof(*v));
    }
    if (const auto* v = std::get_if<double>(&value)) {
        hash = hashString(hash, "d");
        // -0.0 归一成 0.0，否则两个数值相等的设置会得到不同哈希。
        const double normalized = *v == 0.0 ? 0.0 : *v;
        return fnv1a(hash, &normalized, sizeof(normalized));
    }
    if (const auto* v = std::get_if<std::string>(&value)) {
        hash = hashString(hash, "s");
        return hashString(hash, *v);
    }
    if (const auto* v = std::get_if<std::vector<uint64_t>>(&value)) {
        hash = hashString(hash, "a");
        for (const uint64_t element : *v) {
            hash = fnv1a(hash, &element, sizeof(element));
        }
        return hash;
    }
    return hash;
}

std::optional<double> numericValue(const Value& value) {
    if (const auto* v = std::get_if<int64_t>(&value)) {
        return static_cast<double>(*v);
    }
    if (const auto* v = std::get_if<uint64_t>(&value)) {
        return static_cast<double>(*v);
    }
    if (const auto* v = std::get_if<double>(&value)) {
        return *v;
    }
    return std::nullopt;
}

[[noreturn]] void missingKey(std::string_view key, std::string_view expected) {
    // schema 是权威：解析后每个 schema 键都存在且类型正确。取不到只能是
    // importer 问了一个自己没声明的键。
    throw std::logic_error("ResolvedSettings has no " + std::string{ expected } + " setting '" +
                           std::string{ key } + "'");
}

} // namespace

bool settingMatchesDescriptor(const SettingDescriptor& descriptor, const Value& value) {
    if (value.index() != descriptor.default_value.index()) {
        return false;
    }
    if (const auto* text = std::get_if<std::string>(&value);
            text != nullptr && !descriptor.allowed.empty()) {
        if (std::ranges::find(descriptor.allowed, *text) == descriptor.allowed.end()) {
            return false;
        }
    }
    if (const auto number = numericValue(value)) {
        if (!std::isfinite(*number)) {
            return false;
        }
        if (descriptor.min && *number < *descriptor.min) {
            return false;
        }
        if (descriptor.max && *number > *descriptor.max) {
            return false;
        }
    }
    return true;
}

ResolvedSettings resolveSettings(const std::vector<SettingDescriptor>& schema,
        const AssetSettings& settings) {
    ResolvedSettings resolved;

    // 以 schema 为枚举源，不是以磁盘内容 —— 保证每个声明过的键都有值。
    for (const SettingDescriptor& descriptor : schema) {
        // 前缀族：键集在编译期未知，所以没有默认值可填 ——
        // 把 authored/inferred 里所有匹配前缀的键收进来即可。
        if (descriptor.is_prefix) {
            const auto take = [&](const std::string& key, const Value& value,
                                       SettingLayer layer) {
                if (!key.starts_with(descriptor.key) || resolved.m_values.contains(key)) {
                    return;
                }
                if (!settingMatchesDescriptor(descriptor, value)) {
                    resolved.m_issues.push_back({ key,
                        layer == SettingLayer::Authored ? SettingIssueKind::InvalidAuthored
                                                        : SettingIssueKind::InvalidInferred,
                        "the value does not match the schema for prefix '" + descriptor.key +
                                "'" });
                    return;
                }
                resolved.m_layers.emplace(key, layer);
                resolved.m_values.emplace(key, value);
            };
            // authored 优先于 inferred，与单键路径一致。
            for (const auto& [key, value] : settings.authored) {
                take(key, value, SettingLayer::Authored);
            }
            for (const auto& [key, entry] : settings.inferred) {
                take(key, entry.value, SettingLayer::Inferred);
            }
            continue;
        }

        Value value = descriptor.default_value;
        SettingLayer layer = SettingLayer::Default;

        if (const auto authored = settings.authored.find(descriptor.key);
                authored != settings.authored.end()) {
            if (settingMatchesDescriptor(descriptor, authored->second)) {
                value = authored->second;
                layer = SettingLayer::Authored;
            } else {
                resolved.m_issues.push_back({ descriptor.key,
                    SettingIssueKind::InvalidAuthored,
                    "the authored value does not match the schema; using the default" });
            }
        } else if (const auto inferred = settings.inferred.find(descriptor.key);
                   inferred != settings.inferred.end()) {
            if (settingMatchesDescriptor(descriptor, inferred->second.value)) {
                value = inferred->second.value;
                layer = SettingLayer::Inferred;
            } else {
                resolved.m_issues.push_back({ descriptor.key,
                    SettingIssueKind::InvalidInferred,
                    "the inferred value does not match the schema; using the default" });
            }
        }

        resolved.m_layers.emplace(descriptor.key, layer);
        resolved.m_values.emplace(descriptor.key, std::move(value));
    }

    // schema 不认识的 Authored 键：保留原值不动，只报告。用旧版编辑器打开新版
    // 项目不该把没认出来的设置抹掉。
    for (const auto& [key, value] : settings.authored) {
        if (!resolved.m_values.contains(key)) {
            resolved.m_issues.push_back({ key, SettingIssueKind::UnknownKey,
                "no importer setting is declared for this key" });
        }
    }
    return resolved;
}

bool ResolvedSettings::has(std::string_view key) const {
    return m_values.contains(std::string{ key });
}

SettingLayer ResolvedSettings::layerOf(std::string_view key) const {
    const auto found = m_layers.find(std::string{ key });
    return found == m_layers.end() ? SettingLayer::Default : found->second;
}

std::vector<std::pair<std::string, Value>> ResolvedSettings::withPrefix(
        std::string_view prefix) const {
    std::vector<std::pair<std::string, Value>> matches;
    for (const auto& [key, value] : m_values) {
        if (key.starts_with(prefix)) {
            matches.emplace_back(key, value);
        }
    }
    // 排序，让枚举顺序与 unordered_map 的遍历序无关。
    std::ranges::sort(matches, [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    return matches;
}

bool ResolvedSettings::getBool(std::string_view key) const {
    const auto found = m_values.find(std::string{ key });
    if (found == m_values.end()) {
        missingKey(key, "bool");
    }
    if (const auto* value = std::get_if<bool>(&found->second)) {
        return *value;
    }
    missingKey(key, "bool");
}

int64_t ResolvedSettings::getInt(std::string_view key) const {
    const auto found = m_values.find(std::string{ key });
    if (found == m_values.end()) {
        missingKey(key, "int");
    }
    if (const auto* value = std::get_if<int64_t>(&found->second)) {
        return *value;
    }
    if (const auto* value = std::get_if<uint64_t>(&found->second)) {
        return static_cast<int64_t>(*value);
    }
    missingKey(key, "int");
}

double ResolvedSettings::getDouble(std::string_view key) const {
    const auto found = m_values.find(std::string{ key });
    if (found == m_values.end()) {
        missingKey(key, "double");
    }
    if (const auto number = numericValue(found->second)) {
        return *number;
    }
    missingKey(key, "double");
}

const std::string& ResolvedSettings::getString(std::string_view key) const {
    const auto found = m_values.find(std::string{ key });
    if (found == m_values.end()) {
        missingKey(key, "string");
    }
    if (const auto* value = std::get_if<std::string>(&found->second)) {
        return *value;
    }
    missingKey(key, "string");
}

uint64_t ResolvedSettings::hash() const {
    // 键排序，保证哈希与遍历序无关。
    std::vector<const std::string*> keys;
    keys.reserve(m_values.size());
    for (const auto& [key, value] : m_values) {
        keys.push_back(&key);
    }
    std::ranges::sort(keys, [](const std::string* left, const std::string* right) {
        return *left < *right;
    });

    uint64_t hash = kFnvOffset;
    for (const std::string* key : keys) {
        hash = hashString(hash, *key);
        hash = hashValue(hash, m_values.at(*key));
    }
    return hash;
}

}
