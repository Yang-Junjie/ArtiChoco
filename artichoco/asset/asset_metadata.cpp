#include "asset_metadata.h"

#include "detail/metadata_yaml.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ranges>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace arti::asset {

using detail::readDependencies;
using detail::readProperties;
using detail::serializeDependencies;
using detail::serializeProperties;
using detail::serializeValue;

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

// local_id 不进文件名（artifact 按 handle 命名），所以只要求它是可显示、
// 可作为查询键的单个片段：非空时不含路径分隔符与空白。
bool isValidLocalId(std::string_view local_id) {
    if (local_id.empty()) {
        return true; // 空 = 源文件本身就是唯一产出
    }
    for (const char character : local_id) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '/' || c == '\\' || std::isspace(c) != 0) {
            return false;
        }
    }
    return true;
}

namespace {

bool valuesAreFinite(const std::unordered_map<std::string, Value>& map) {
    for (const auto& [key, value] : map) {
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

}

bool isValidAssetRecord(const AssetRecord& record) {
    if (!record.handle.isValid() || record.type.empty() ||
            !isValidLocalId(record.local_id) ||
            !isSafeAssetRelativePath(record.artifact_path)) {
        return false;
    }
    for (core::UUID dependency : record.dependencies) {
        if (!dependency.isValid()) {
            return false;
        }
    }
    return valuesAreFinite(record.properties);
}

bool isValidSourceMetadata(const SourceMetadata& metadata) {
    if (!isSafeAssetRelativePath(metadata.source_path)) {
        return false;
    }
    if (!valuesAreFinite(metadata.settings.authored)) {
        return false;
    }
    for (const auto& [key, inferred] : metadata.settings.inferred) {
        if (key.empty()) {
            return false;
        }
        if (const auto* number = std::get_if<double>(&inferred.value);
                number != nullptr && !std::isfinite(*number)) {
            return false;
        }
    }

    // identity 唯一性：同一个源里 local_id 不能重复，handle 也不能重复。
    std::vector<std::string> local_ids;
    std::vector<core::UUID> handles;
    for (const AssetRecord& record : metadata.assets) {
        if (!isValidAssetRecord(record)) {
            return false;
        }
        if (std::ranges::find(local_ids, record.local_id) != local_ids.end()) {
            return false;
        }
        if (std::ranges::find(handles, record.handle) != handles.end()) {
            return false;
        }
        local_ids.push_back(record.local_id);
        handles.push_back(record.handle);
    }
    return true;
}

bool isValidAssetMetadata(const AssetMetadata& metadata) {
    if (!metadata.handle.isValid() || metadata.type.empty() ||
            !isValidLocalId(metadata.local_id) ||
            !isSafeAssetRelativePath(metadata.source_path) ||
            !isSafeAssetRelativePath(metadata.artifact_path)) {
        return false;
    }

    for (core::UUID dependency : metadata.dependencies) {
        if (!dependency.isValid()) {
            return false;
        }
    }
    return valuesAreFinite(metadata.properties);
}

std::vector<AssetMetadata> expandSourceMetadata(const SourceMetadata& metadata) {
    std::vector<AssetMetadata> expanded;
    expanded.reserve(metadata.assets.size());
    for (const AssetRecord& record : metadata.assets) {
        AssetMetadata entry;
        entry.handle = record.handle;
        entry.type = record.type;
        entry.local_id = record.local_id;
        entry.source_path = metadata.source_path;
        entry.artifact_path = record.artifact_path;
        entry.dependencies = record.dependencies;
        entry.properties = record.properties;
        expanded.push_back(std::move(entry));
    }
    return expanded;
}

std::optional<std::string> serializeSourceMetadata(const SourceMetadata& metadata) {
    if (!isValidSourceMetadata(metadata)) {
        return std::nullopt;
    }

    try {
        YAML::Node source;
        source["Path"] = metadata.source_path.lexically_normal().generic_string();
        source["ContentHash"] = metadata.fingerprint.content_hash;
        source["Size"] = metadata.fingerprint.size;

        YAML::Node importer;
        importer["Name"] = metadata.importer.name;
        importer["Version"] = metadata.importer.version;

        YAML::Node authored{ YAML::NodeType::Map };
        for (const auto& [key, value] : metadata.settings.authored) {
            authored[key] = serializeValue(value);
        }
        YAML::Node inferred{ YAML::NodeType::Map };
        for (const auto& [key, entry] : metadata.settings.inferred) {
            YAML::Node node;
            node["Value"] = serializeValue(entry.value);
            node["By"] = entry.by.lexically_normal().generic_string();
            node["Usage"] = entry.usage;
            inferred[key] = node;
        }
        YAML::Node settings;
        settings["Authored"] = authored;
        settings["Inferred"] = inferred;
        settings["ResolvedHash"] = metadata.settings_hash;

        YAML::Node assets{ YAML::NodeType::Sequence };
        for (const AssetRecord& record : metadata.assets) {
            YAML::Node node;
            node["LocalId"] = record.local_id;
            node["Handle"] = record.handle.toString();
            node["Type"] = record.type;
            node["ArtifactPath"] = record.artifact_path.lexically_normal().generic_string();

            node["Properties"] = serializeProperties(record.properties);
            node["Dependencies"] = serializeDependencies(record.dependencies);
            assets.push_back(node);
        }

        YAML::Node root;
        root["Version"] = metadata.version;
        root["Source"] = source;
        root["Importer"] = importer;
        root["Settings"] = settings;
        root["Assets"] = assets;

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

std::optional<SourceMetadata> deserializeSourceMetadata(std::string_view text) {
    try {
        const YAML::Node root = YAML::Load(std::string{ text });
        const YAML::Node source = root["Source"];
        if (!root["Version"] || !source || !source.IsMap() || !source["Path"]) {
            return std::nullopt;
        }

        SourceMetadata metadata;
        metadata.version = root["Version"].as<uint32_t>();
        metadata.source_path = source["Path"].as<std::string>();
        if (const auto hash = source["ContentHash"]) {
            metadata.fingerprint.content_hash = hash.as<uint64_t>();
        }
        if (const auto size = source["Size"]) {
            metadata.fingerprint.size = size.as<uint64_t>();
        }

        if (const YAML::Node importer = root["Importer"]; importer && importer.IsMap()) {
            if (const auto name = importer["Name"]) {
                metadata.importer.name = name.as<std::string>();
            }
            if (const auto version = importer["Version"]) {
                metadata.importer.version = version.as<uint32_t>();
            }
        }

        if (const YAML::Node settings = root["Settings"]; settings && settings.IsMap()) {
            if (!readProperties(settings["Authored"], metadata.settings.authored)) {
                return std::nullopt;
            }
            if (const auto hash = settings["ResolvedHash"]) {
                metadata.settings_hash = hash.as<uint64_t>();
            }
            if (const YAML::Node inferred = settings["Inferred"]) {
                if (!inferred.IsMap()) {
                    return std::nullopt;
                }
                for (const auto& entry : inferred) {
                    const YAML::Node node = entry.second;
                    if (!node.IsMap() || !node["Value"]) {
                        return std::nullopt;
                    }
                    auto value = detail::deserializeValue(node["Value"]);
                    if (!value) {
                        return std::nullopt;
                    }
                    InferredSetting setting;
                    setting.value = std::move(*value);
                    if (const auto by = node["By"]) {
                        setting.by = by.as<std::string>();
                    }
                    if (const auto usage = node["Usage"]) {
                        setting.usage = usage.as<std::string>();
                    }
                    metadata.settings.inferred.emplace(entry.first.as<std::string>(),
                            std::move(setting));
                }
            }
        }

        const YAML::Node assets = root["Assets"];
        if (assets) {
            if (!assets.IsSequence()) {
                return std::nullopt;
            }
            for (const YAML::Node& node : assets) {
                if (!node.IsMap() || !node["Handle"] || !node["Type"] || !node["ArtifactPath"]) {
                    return std::nullopt;
                }
                const auto handle = core::UUID::fromString(node["Handle"].as<std::string>());
                if (!handle) {
                    return std::nullopt;
                }

                AssetRecord record;
                record.handle = *handle;
                record.type = node["Type"].as<std::string>();
                record.artifact_path = node["ArtifactPath"].as<std::string>();
                if (const auto local_id = node["LocalId"]) {
                    record.local_id = local_id.as<std::string>();
                }
                if (!readProperties(node["Properties"], record.properties) ||
                        !readDependencies(node["Dependencies"], record.dependencies)) {
                    return std::nullopt;
                }
                metadata.assets.push_back(std::move(record));
            }
        }

        if (!isValidSourceMetadata(metadata)) {
            return std::nullopt;
        }
        return metadata;
    } catch (...) {
        return std::nullopt;
    }
}

}
