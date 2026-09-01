#pragma once
#include "artichoco/asset/asset_metadata.h"

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

// AssetMetadata 家族共用的 YAML 编解码。私有实现细节：它 include 了 yaml-cpp，不该出现在
// 任何对外的头里。
//
// 存在的理由是 sidecar（.meta）和打包 manifest（.artimanifest）写的是同一批字段
// —— Properties 里的 Value 变体、Dependencies 的 handle 列表。两份实现迟早会漂移，
// 而漂移的表现是「编辑器写出来的东西运行时读不回来」，最难查的那一类。
namespace arti::asset::detail {

// Value 变体带标签存：YAML 本身分不清 42 是 int64 还是 uint64，而 importer 的设置里
// 两者语义不同。
inline constexpr std::string_view kBoolTag = "!arti/bool";
inline constexpr std::string_view kIntTag = "!arti/int";
inline constexpr std::string_view kUintTag = "!arti/uint";
inline constexpr std::string_view kDoubleTag = "!arti/double";
inline constexpr std::string_view kStringTag = "!arti/string";

inline YAML::Node serializeValue(const Value& value) {
    YAML::Node node;
    if (const auto* v = std::get_if<bool>(&value)) {
        node = *v;
        node.SetTag(std::string{ kBoolTag });
    } else if (const auto* v = std::get_if<int64_t>(&value)) {
        node = *v;
        node.SetTag(std::string{ kIntTag });
    } else if (const auto* v = std::get_if<uint64_t>(&value)) {
        node = *v;
        node.SetTag(std::string{ kUintTag });
    } else if (const auto* v = std::get_if<double>(&value)) {
        node = *v;
        node.SetTag(std::string{ kDoubleTag });
    } else if (const auto* v = std::get_if<std::string>(&value)) {
        node = *v;
        node.SetTag(std::string{ kStringTag });
    } else if (const auto* v = std::get_if<std::vector<uint64_t>>(&value)) {
        node = YAML::Node{ YAML::NodeType::Sequence };
        for (uint64_t element: *v) {
            YAML::Node element_node{ element };
            element_node.SetTag(std::string{ kUintTag });
            node.push_back(element_node);
        }
    }
    return node;
}

inline std::optional<Value> deserializeValue(const YAML::Node& node) {
    if (const std::string tag = node.Tag(); tag == kBoolTag) {
        return Value{ node.as<bool>() };
    } else if (tag == kIntTag) {
        return Value{ node.as<int64_t>() };
    } else if (tag == kUintTag) {
        return Value{ node.as<uint64_t>() };
    } else if (tag == kDoubleTag) {
        return Value{ node.as<double>() };
    } else if (tag == kStringTag) {
        return Value{ node.as<std::string>() };
    }
    if (node.IsSequence()) {
        std::vector<uint64_t> elements;
        for (const YAML::Node& element: node) {
            elements.push_back(element.as<uint64_t>());
        }
        return Value{ std::move(elements) };
    }
    return std::nullopt;
}

inline YAML::Node serializeProperties(const std::unordered_map<std::string, Value>& properties) {
    YAML::Node node{ YAML::NodeType::Map };
    for (const auto& [key, value]: properties) {
        node[key] = serializeValue(value);
    }
    return node;
}

// handle 列表写成 flow 序列（一行一串），不然 .meta 会被撑得没法读。
inline YAML::Node serializeDependencies(const std::vector<core::UUID>& dependencies) {
    YAML::Node node{ YAML::NodeType::Sequence };
    for (core::UUID dependency: dependencies) {
        node.push_back(dependency.toString());
    }
    node.SetStyle(YAML::EmitterStyle::Flow);
    return node;
}

// 缺字段（!node）视为空，不是错误 —— 老版本写出来的文件里可能压根没有这一项。
inline bool readProperties(const YAML::Node& node, std::unordered_map<std::string, Value>& out) {
    if (!node) {
        return true;
    }
    if (!node.IsMap()) {
        return false;
    }
    for (const auto& entry: node) {
        auto value = deserializeValue(entry.second);
        if (!value) {
            return false;
        }
        out.emplace(entry.first.as<std::string>(), std::move(*value));
    }
    return true;
}

inline bool readDependencies(const YAML::Node& node, std::vector<core::UUID>& out) {
    if (!node) {
        return true;
    }
    if (!node.IsSequence()) {
        return false;
    }
    for (const YAML::Node& entry: node) {
        const auto handle = core::UUID::fromString(entry.as<std::string>());
        if (!handle) {
            return false;
        }
        out.push_back(*handle);
    }
    return true;
}

} // namespace arti::asset::detail
