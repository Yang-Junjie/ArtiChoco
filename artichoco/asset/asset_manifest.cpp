#include "asset_manifest.h"

#include "asset_log.h"
#include "detail/metadata_yaml.h"

#include <unordered_set>
#include <utility>

namespace arti::asset {

std::optional<std::string> serializeAssetManifest(const AssetManifest& manifest) {
    try {
        YAML::Node assets{ YAML::NodeType::Sequence };
        for (const AssetMetadata& entry: manifest.assets) {
            if (!isValidAssetMetadata(entry)) {
                getLogChannel().error("Refusing to write the Asset manifest: entry {} is invalid",
                        entry.handle.toString());
                return std::nullopt;
            }

            YAML::Node node;
            node["Handle"] = entry.handle.toString();
            node["Type"] = entry.type;
            node["LocalId"] = entry.local_id;
            // SourcePath 在打包产物里已经没有对应文件了，但它是资产身份的一半
            // （identity 是 source_path + local_id），而且日志和报错全靠它讲人话。
            node["SourcePath"] = entry.source_path.lexically_normal().generic_string();
            node["ArtifactPath"] = entry.artifact_path.lexically_normal().generic_string();
            node["Properties"] = detail::serializeProperties(entry.properties);
            node["Dependencies"] = detail::serializeDependencies(entry.dependencies);
            assets.push_back(node);
        }

        YAML::Node root;
        root["Version"] = manifest.version;
        root["Assets"] = assets;

        YAML::Emitter emitter;
        emitter << root;
        if (!emitter.good()) {
            getLogChannel().error("Failed to emit the Asset manifest: {}", emitter.GetLastError());
            return std::nullopt;
        }
        return std::string{ emitter.c_str() };
    } catch (const std::exception& exception) {
        getLogChannel().error("Failed to serialize the Asset manifest: {}", exception.what());
        return std::nullopt;
    }
}

std::optional<AssetManifest> deserializeAssetManifest(std::string_view text) {
    try {
        const YAML::Node root = YAML::Load(std::string{ text });
        if (!root.IsMap() || !root["Version"]) {
            getLogChannel().error("The Asset manifest has no Version");
            return std::nullopt;
        }

        AssetManifest manifest;
        manifest.version = root["Version"].as<std::uint32_t>();
        if (manifest.version != kAssetManifestVersion) {
            // 不做向前兼容：manifest 是打包产物，和它一起发布的 exe 版本是配套的。
            getLogChannel().error("The Asset manifest is version {} but this build reads {}",
                    manifest.version, kAssetManifestVersion);
            return std::nullopt;
        }

        const YAML::Node assets = root["Assets"];
        if (assets && !assets.IsSequence()) {
            getLogChannel().error("The Asset manifest's Assets is not a sequence");
            return std::nullopt;
        }

        std::unordered_set<core::UUID> seen;
        for (const YAML::Node& node: assets) {
            if (!node.IsMap() || !node["Handle"] || !node["Type"] || !node["SourcePath"] ||
                    !node["ArtifactPath"]) {
                getLogChannel().error("The Asset manifest has an entry missing required fields");
                return std::nullopt;
            }
            const auto handle = core::UUID::fromString(node["Handle"].as<std::string>());
            if (!handle) {
                getLogChannel().error("The Asset manifest has a malformed handle '{}'",
                        node["Handle"].as<std::string>());
                return std::nullopt;
            }

            AssetMetadata entry;
            entry.handle = *handle;
            entry.type = node["Type"].as<std::string>();
            entry.source_path = node["SourcePath"].as<std::string>();
            entry.artifact_path = node["ArtifactPath"].as<std::string>();
            if (const auto local_id = node["LocalId"]) {
                entry.local_id = local_id.as<std::string>();
            }
            if (!detail::readProperties(node["Properties"], entry.properties) ||
                    !detail::readDependencies(node["Dependencies"], entry.dependencies)) {
                getLogChannel().error("The Asset manifest entry {} has bad properties or "
                                      "dependencies",
                        entry.handle.toString());
                return std::nullopt;
            }
            if (!isValidAssetMetadata(entry)) {
                getLogChannel().error("The Asset manifest entry {} failed validation",
                        entry.handle.toString());
                return std::nullopt;
            }
            // 重复 handle 会让 catalog 静默丢掉后来者（先到者胜），那种「少一个资产」
            // 极难查，所以在这里就拒。
            if (!seen.insert(entry.handle).second) {
                getLogChannel().error("The Asset manifest lists handle {} twice",
                        entry.handle.toString());
                return std::nullopt;
            }
            manifest.assets.push_back(std::move(entry));
        }
        return manifest;
    } catch (const std::exception& exception) {
        getLogChannel().error("Failed to parse the Asset manifest: {}", exception.what());
        return std::nullopt;
    }
}

} // namespace arti::asset
