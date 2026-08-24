#include "asset_metadata.h"

#include <cmath>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace arti::asset {
namespace {

constexpr std::string_view bool_tag = "!arti/bool";
constexpr std::string_view int_tag = "!arti/int";
constexpr std::string_view uint_tag = "!arti/uint";
constexpr std::string_view double_tag = "!arti/double";
constexpr std::string_view string_tag = "!arti/string";

YAML::Node serializeValue(const Value& value) {
    YAML::Node node;
    if (const auto* v = std::get_if<bool>(&value)) {
        node = *v;
        node.SetTag(std::string{ bool_tag });
    } else if (const auto* v = std::get_if<int64_t>(&value)) {
        node = *v;
        node.SetTag(std::string{ int_tag });
    } else if (const auto* v = std::get_if<uint64_t>(&value)) {
        node = *v;
        node.SetTag(std::string{ uint_tag });
    } else if (const auto* v = std::get_if<double>(&value)) {
        node = *v;
        node.SetTag(std::string{ double_tag });
    } else if (const auto* v = std::get_if<std::string>(&value)) {
        node = *v;
        node.SetTag(std::string{ string_tag });
    } else if (const auto* v = std::get_if<std::vector<uint64_t>>(&value)) {
        node = YAML::Node{ YAML::NodeType::Sequence };
        for (uint64_t element : *v) {
            YAML::Node element_node{ element };
            element_node.SetTag(std::string{ uint_tag });
            node.push_back(element_node);
        }
    }
    return node;
}

std::optional<Value> deserializeValue(const YAML::Node& node) {
    if (const std::string tag = node.Tag(); tag == bool_tag) {
        return Value{ node.as<bool>() };
    } else if (tag == int_tag) {
        return Value{ node.as<int64_t>() };
    } else if (tag == uint_tag) {
        return Value{ node.as<uint64_t>() };
    } else if (tag == double_tag) {
        return Value{ node.as<double>() };
    } else if (tag == string_tag) {
        return Value{ node.as<std::string>() };
    }
    if (node.IsSequence()) {
        std::vector<uint64_t> elements;
        for (const YAML::Node& element : node) {
            elements.push_back(element.as<uint64_t>());
        }
        return Value{ std::move(elements) };
    }
    return std::nullopt;
}

}

bool isSafeAssetRelativePath(const std::filesystem::path& path, bool allow_empty) {
    if (path.empty()) {
        return allow_empty;
    }
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
        return false;
    }

    const std::filesystem::path normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        return false;
    }
    for (const auto& component : normalized) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool isValidAssetMetadata(const AssetMetadata& metadata) {
    if (!metadata.handle.isValid() || metadata.type.empty() ||
            !isSafeAssetRelativePath(metadata.source_path) ||
            !isSafeAssetRelativePath(metadata.artifact_path)) {
        return false;
    }

    for (core::UUID dependency : metadata.dependencies) {
        if (!dependency.isValid()) {
            return false;
        }
    }

    for (const auto& [key, value] : metadata.properties) {
        if (key.empty()) {
            return false;
        }
        if (const auto* number = std::get_if<double>(&value);
                number != nullptr && !std::isfinite(*number)) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> serializeAssetMetadata(const AssetMetadata& metadata) {
    if (!isValidAssetMetadata(metadata)) {
        return std::nullopt;
    }

    try {
        YAML::Node asset;
        asset["Handle"] = metadata.handle.toString();
        asset["Type"] = metadata.type;
        asset["SourcePath"] = metadata.source_path.lexically_normal().generic_string();
        asset["ArtifactPath"] = metadata.artifact_path.lexically_normal().generic_string();

        YAML::Node properties{ YAML::NodeType::Map };
        for (const auto& [key, value] : metadata.properties) {
            properties[key] = serializeValue(value);
        }
        asset["Properties"] = properties;

        YAML::Node dependencies{ YAML::NodeType::Sequence };
        for (core::UUID dependency : metadata.dependencies) {
            dependencies.push_back(dependency.toString());
        }
        asset["Dependencies"] = dependencies;

        YAML::Node root;
        root["Asset"] = asset;

        YAML::Emitter emitter;
        emitter << root;
        if (!emitter.good()) {
            return std::nullopt;
        }
        return std::string{ emitter.c_str() };
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<AssetMetadata> deserializeAssetMetadata(std::string_view text) {
    try {
        const YAML::Node root = YAML::Load(std::string{ text });
        const YAML::Node asset = root["Asset"];
        if (!asset || !asset.IsMap() || !asset["Handle"] || !asset["Type"] ||
                !asset["SourcePath"] || !asset["ArtifactPath"]) {
            return std::nullopt;
        }

        const auto handle = core::UUID::fromString(asset["Handle"].as<std::string>());
        if (!handle) {
            return std::nullopt;
        }

        AssetMetadata metadata;
        metadata.handle = *handle;
        metadata.type = asset["Type"].as<std::string>();
        metadata.source_path = asset["SourcePath"].as<std::string>();
        metadata.artifact_path = asset["ArtifactPath"].as<std::string>();

        const YAML::Node properties = asset["Properties"];
        if (properties) {
            if (!properties.IsMap()) {
                return std::nullopt;
            }
            for (const auto& entry : properties) {
                const std::string key = entry.first.as<std::string>();
                auto value = deserializeValue(entry.second);
                if (!value) {
                    return std::nullopt;
                }
                metadata.properties.emplace(key, std::move(*value));
            }
        }

        const YAML::Node dependencies = asset["Dependencies"];
        if (dependencies) {
            if (!dependencies.IsSequence()) {
                return std::nullopt;
            }
            for (const YAML::Node& entry : dependencies) {
                const auto handle = core::UUID::fromString(entry.as<std::string>());
                if (!handle) {
                    return std::nullopt;
                }
                metadata.dependencies.push_back(*handle);
            }
        }

        if (!isValidAssetMetadata(metadata)) {
            return std::nullopt;
        }
        return metadata;
    } catch (...) {
        return std::nullopt;
    }
}

}
