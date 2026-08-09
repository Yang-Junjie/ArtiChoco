#include "asset_database.h"

#include "asset_log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <ranges>
#include <system_error>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace arti::asset {
namespace {

constexpr std::string_view bool_tag = "!arti/bool";
constexpr std::string_view int_tag = "!arti/int";
constexpr std::string_view uint_tag = "!arti/uint";
constexpr std::string_view double_tag = "!arti/double";
constexpr std::string_view string_tag = "!arti/string";

std::string normalizeExtension(std::string_view extension) {
    if (extension.empty()) {
        return {};
    }

    std::string normalized{ extension };
    if (normalized.front() != '.') {
        normalized.insert(normalized.begin(), '.');
    }
    std::ranges::transform(normalized, normalized.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return normalized == "." ? std::string{} : normalized;
}

YAML::Node serializeValue(const AssetMetadataValue& value) {
    YAML::Node node;
    if (value.is<bool>()) {
        node = value.get<bool>();
        node.SetTag(std::string{ bool_tag });
    } else if (value.is<int64_t>()) {
        node = value.get<int64_t>();
        node.SetTag(std::string{ int_tag });
    } else if (value.is<uint64_t>()) {
        node = value.get<uint64_t>();
        node.SetTag(std::string{ uint_tag });
    } else if (value.is<double>()) {
        node = value.get<double>();
        node.SetTag(std::string{ double_tag });
    } else {
        node = value.get<std::string>();
        node.SetTag(std::string{ string_tag });
    }
    return node;
}

std::optional<AssetMetadataValue> deserializeValue(const YAML::Node& node, std::string& error) {
    if (!node.IsScalar()) {
        error = "Asset metadata properties must be scalar values.";
        return std::nullopt;
    }

    try {
        const std::string tag = node.Tag();
        if (tag == bool_tag) {
            return AssetMetadataValue{ node.as<bool>() };
        }
        if (tag == int_tag) {
            return AssetMetadataValue{ node.as<int64_t>() };
        }
        if (tag == uint_tag) {
            return AssetMetadataValue{ node.as<uint64_t>() };
        }
        if (tag == double_tag) {
            return AssetMetadataValue{ node.as<double>() };
        }
        if (tag == string_tag || tag == "?" || tag == "!") {
            return AssetMetadataValue{ node.as<std::string>() };
        }
        error = "Asset metadata property uses an unknown YAML tag: " + tag;
    } catch (const YAML::Exception& exception) {
        error = exception.what();
    }
    return std::nullopt;
}

std::optional<std::string> serializeMetadata(const AssetMetadata& metadata, std::string& error) {
    if (!isValidAssetMetadata(metadata)) {
        error = "AssetMetadata is invalid.";
        return std::nullopt;
    }

    try {
        YAML::Node asset;
        asset["Handle"] = metadata.handle.toString();
        asset["Type"] = std::string{ metadata.type.name() };
        asset["SourcePath"] = metadata.source_path.lexically_normal().generic_string();
        asset["ArtifactPath"] = metadata.artifact_path.lexically_normal().generic_string();

        YAML::Node dependencies{ YAML::NodeType::Sequence };
        for (core::UUID dependency: metadata.dependencies) {
            dependencies.push_back(dependency.toString());
        }
        asset["Dependencies"] = dependencies;

        YAML::Node properties{ YAML::NodeType::Map };
        for (const auto& [key, value]: metadata.properties) {
            properties[key] = serializeValue(value);
        }
        asset["Properties"] = properties;

        YAML::Node root;
        root["Asset"] = asset;
        YAML::Emitter emitter;
        emitter << root;
        if (!emitter.good()) {
            error = emitter.GetLastError();
            return std::nullopt;
        }
        return std::string{ emitter.c_str() };
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::optional<AssetMetadata> deserializeMetadata(std::string_view text, std::string& error) {
    try {
        const YAML::Node root = YAML::Load(std::string{ text });
        const YAML::Node asset = root["Asset"];
        if (!asset || !asset.IsMap() || !asset["Handle"] || !asset["Type"] ||
                !asset["SourcePath"] || !asset["ArtifactPath"]) {
            error = "Asset metadata is missing a required field.";
            return std::nullopt;
        }

        const auto handle = core::UUID::fromString(asset["Handle"].as<std::string>());
        if (!handle) {
            error = "Asset metadata contains an invalid handle.";
            return std::nullopt;
        }

        AssetMetadata metadata;
        metadata.handle = *handle;
        metadata.type = AssetType{ asset["Type"].as<std::string>() };
        metadata.source_path = asset["SourcePath"].as<std::string>();
        metadata.artifact_path = asset["ArtifactPath"].as<std::string>();

        const YAML::Node dependencies = asset["Dependencies"];
        if (dependencies) {
            if (!dependencies.IsSequence()) {
                error = "Asset metadata Dependencies must be a sequence.";
                return std::nullopt;
            }
            for (const YAML::Node& dependency_node: dependencies) {
                const auto dependency = core::UUID::fromString(dependency_node.as<std::string>());
                if (!dependency) {
                    error = "Asset metadata contains an invalid dependency.";
                    return std::nullopt;
                }
                metadata.dependencies.push_back(*dependency);
            }
        }

        const YAML::Node properties = asset["Properties"];
        if (properties) {
            if (!properties.IsMap()) {
                error = "Asset metadata Properties must be a map.";
                return std::nullopt;
            }
            for (const auto& entry: properties) {
                const std::string key = entry.first.as<std::string>();
                if (key.empty()) {
                    error = "Asset metadata property keys cannot be empty.";
                    return std::nullopt;
                }
                auto value = deserializeValue(entry.second, error);
                if (!value) {
                    return std::nullopt;
                }
                metadata.properties.emplace(key, std::move(*value));
            }
        }

        if (!isValidAssetMetadata(metadata)) {
            error = "Asset metadata contains an invalid value or path.";
            return std::nullopt;
        }
        return metadata;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

bool replaceFile(const std::filesystem::path& temporary_path,
        const std::filesystem::path& destination_path, std::string& error_message) {
    std::error_code error;
    std::filesystem::rename(temporary_path, destination_path, error);
    if (!error) {
        return true;
    }

    const std::error_code first_error = error;
    error.clear();
    if (!std::filesystem::exists(destination_path, error) || error) {
        error_message = error ? error.message() : first_error.message();
        return false;
    }

    std::filesystem::path backup_path = destination_path;
    backup_path += ".bak." + core::UUID::generate().toString();
    std::filesystem::rename(destination_path, backup_path, error);
    if (error) {
        error_message = error.message();
        return false;
    }

    std::filesystem::rename(temporary_path, destination_path, error);
    if (error) {
        error_message = error.message();
        std::error_code restore_error;
        std::filesystem::rename(backup_path, destination_path, restore_error);
        return false;
    }

    std::error_code ignored;
    std::filesystem::remove(backup_path, ignored);
    return true;
}

AssetImportResult importFailure(const std::filesystem::path& path, std::string message) {
    getLogChannel().error("Asset import failed for '{}': {}", path.string(), message);
    return { .error = std::move(message) };
}

} // namespace

bool AssetDatabase::open(std::filesystem::path assets_root, std::filesystem::path artifacts_root) {
    close();
    if (assets_root.empty() || artifacts_root.empty()) {
        getLogChannel().error("Failed to open AssetDatabase: workspace path is empty");
        return false;
    }

    std::error_code error;
    assets_root = std::filesystem::absolute(assets_root, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve Assets root: {}", error.message());
        return false;
    }
    artifacts_root = std::filesystem::absolute(artifacts_root, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve Artifacts root: {}", error.message());
        return false;
    }

    std::filesystem::create_directories(assets_root, error);
    if (error) {
        getLogChannel().error("Failed to create Assets root '{}': {}", assets_root.string(),
                error.message());
        return false;
    }
    std::filesystem::create_directories(artifacts_root, error);
    if (error) {
        getLogChannel().error("Failed to create Artifacts root '{}': {}", artifacts_root.string(),
                error.message());
        return false;
    }

    m_assets_root = std::move(assets_root);
    m_artifacts_root = std::move(artifacts_root);

    std::filesystem::recursive_directory_iterator iterator{ m_assets_root,
        std::filesystem::directory_options::skip_permission_denied, error };
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const std::filesystem::directory_entry& entry = *iterator;
        if (entry.is_regular_file(error)) {
            std::string relative_name =
                    std::filesystem::relative(entry.path(), m_assets_root, error).generic_string();
            if (!error && relative_name.ends_with(".meta")) {
                relative_name.resize(relative_name.size() - 5);
                const std::filesystem::path source_path{ relative_name };
                MetadataReadResult loaded = readMetadata(source_path);
                if (!loaded.metadata) {
                    getLogChannel().error("Failed to read Asset metadata '{}': {}",
                            entry.path().string(), loaded.error);
                    close();
                    return false;
                }
                if (m_metadata.contains(loaded.metadata->handle) ||
                        m_source_paths.contains(pathKey(loaded.metadata->source_path)) ||
                        m_artifact_paths.contains(pathKey(loaded.metadata->artifact_path))) {
                    getLogChannel().error("Duplicate Asset metadata found at '{}'",
                            entry.path().string());
                    close();
                    return false;
                }
                store(std::move(*loaded.metadata));
            }
        }
        iterator.increment(error);
    }
    if (error) {
        getLogChannel().error("Failed while scanning Assets root '{}': {}", m_assets_root.string(),
                error.message());
        close();
        return false;
    }

    getLogChannel().info("Opened AssetDatabase ({} Assets, source '{}', artifacts '{}')", size(),
            m_assets_root.string(), m_artifacts_root.string());
    return true;
}

void AssetDatabase::close() noexcept {
    m_metadata.clear();
    m_source_paths.clear();
    m_artifact_paths.clear();
    m_assets_root.clear();
    m_artifacts_root.clear();
}

bool AssetDatabase::registerImporter(std::unique_ptr<AssetImporter> importer) {
    if (!importer) {
        getLogChannel().error("Cannot register a null AssetImporter");
        return false;
    }

    std::vector<std::string> declared_extensions;
    try {
        declared_extensions = importer->getSupportedExtensions();
    } catch (const std::exception& exception) {
        getLogChannel().error("AssetImporter failed while declaring extensions: {}",
                exception.what());
        return false;
    } catch (...) {
        getLogChannel().error(
                "AssetImporter threw an unknown exception while declaring extensions");
        return false;
    }

    std::vector<std::string> extensions;
    for (const std::string& extension: declared_extensions) {
        std::string normalized = normalizeExtension(extension);
        if (normalized.empty()) {
            getLogChannel().error("AssetImporter declared an invalid extension '{}'", extension);
            return false;
        }
        if (std::ranges::find(extensions, normalized) == extensions.end()) {
            extensions.push_back(std::move(normalized));
        }
    }
    if (extensions.empty()) {
        getLogChannel().error("AssetImporter did not declare any supported extensions");
        return false;
    }

    for (const ImporterEntry& registered: m_importers) {
        for (const std::string& extension: extensions) {
            if (std::ranges::find(registered.extensions, extension) !=
                    registered.extensions.end()) {
                getLogChannel().error("An AssetImporter is already registered for '{}'", extension);
                return false;
            }
        }
    }

    getLogChannel().info("Registered AssetImporter for {} extension(s)", extensions.size());
    m_importers.push_back({ std::move(importer), std::move(extensions) });
    return true;
}

AssetImportResult AssetDatabase::import(const std::filesystem::path& source_path) {
    if (!isOpen()) {
        return importFailure(source_path, "AssetDatabase is not open.");
    }
    if (!isSafeAssetRelativePath(source_path)) {
        return importFailure(source_path, "Source path is not a safe relative path.");
    }

    const std::filesystem::path normalized_source = source_path.lexically_normal();
    const auto source_file = resolveSourcePath(normalized_source);
    std::error_code error;
    if (!source_file || !std::filesystem::is_regular_file(*source_file, error)) {
        return importFailure(normalized_source,
                error ? error.message() : "Source file does not exist.");
    }

    AssetImporter* importer = findImporter(normalized_source);
    if (importer == nullptr) {
        return importFailure(normalized_source,
                "No AssetImporter is registered for the source extension.");
    }

    AssetMetadata draft;
    bool reimporting = false;
    if (auto existing = findBySourcePath(normalized_source)) {
        draft = std::move(*existing);
        reimporting = true;
    } else {
        draft.handle = core::UUID::generate();
        draft.source_path = normalized_source;
    }

    const core::UUID expected_handle = draft.handle;
    const AssetType expected_type = draft.type;
    AssetImportResult imported;
    try {
        imported = importer->import({
            .source_path = normalized_source,
            .source_file = *source_file,
            .artifacts_root = m_artifacts_root,
            .metadata = draft,
            .reimporting = reimporting,
        });
    } catch (const std::exception& exception) {
        return importFailure(normalized_source,
                std::string{ "AssetImporter threw an exception: " } + exception.what());
    } catch (...) {
        return importFailure(normalized_source, "AssetImporter threw an unknown exception.");
    }

    if (!imported.metadata) {
        return importFailure(normalized_source,
                imported.error.empty() ? "AssetImporter returned no metadata." : imported.error);
    }

    AssetMetadata metadata = std::move(*imported.metadata);
    if (metadata.handle != expected_handle ||
            metadata.source_path.lexically_normal() != normalized_source ||
            (reimporting && metadata.type != expected_type) || !isValidAssetMetadata(metadata)) {
        return importFailure(normalized_source,
                "AssetImporter returned metadata that violates the import contract.");
    }

    const auto artifact_file = resolveArtifactPath(metadata.artifact_path);
    error.clear();
    if (!artifact_file || !std::filesystem::is_regular_file(*artifact_file, error)) {
        return importFailure(normalized_source,
                error ? error.message() : "AssetImporter did not produce its declared Artifact.");
    }
    if (!canStore(metadata)) {
        return importFailure(normalized_source,
                "Imported metadata conflicts with another Asset in the database.");
    }

    std::string write_error;
    if (!writeMetadata(metadata, write_error)) {
        return importFailure(normalized_source, std::move(write_error));
    }

    store(metadata);
    getLogChannel().info("{} Asset '{}' as {} ({})", reimporting ? "Reimported" : "Imported",
            normalized_source.string(), metadata.type.name(), metadata.handle.toString());
    return { .metadata = std::move(metadata) };
}

std::optional<AssetMetadata> AssetDatabase::find(core::UUID handle) const {
    const auto found = m_metadata.find(handle);
    return found == m_metadata.end() ? std::nullopt : std::optional<AssetMetadata>{ found->second };
}

std::optional<AssetMetadata> AssetDatabase::findBySourcePath(
        const std::filesystem::path& source_path) const {
    if (!isSafeAssetRelativePath(source_path)) {
        return std::nullopt;
    }
    const auto source = m_source_paths.find(pathKey(source_path));
    if (source == m_source_paths.end()) {
        return std::nullopt;
    }
    return find(source->second);
}

std::optional<std::filesystem::path> AssetDatabase::resolveSourcePath(
        const std::filesystem::path& relative_path) const {
    if (!isOpen() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_assets_root / relative_path.lexically_normal()).lexically_normal();
}

std::optional<std::filesystem::path> AssetDatabase::resolveArtifactPath(
        const std::filesystem::path& relative_path) const {
    if (!isOpen() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_artifacts_root / relative_path.lexically_normal()).lexically_normal();
}

AssetImporter* AssetDatabase::findImporter(const std::filesystem::path& source_path) {
    const std::string extension = normalizeExtension(source_path.extension().string());
    const auto found = std::ranges::find_if(m_importers, [&extension](const ImporterEntry& entry) {
        return std::ranges::find(entry.extensions, extension) != entry.extensions.end();
    });
    return found == m_importers.end() ? nullptr : found->importer.get();
}

AssetDatabase::MetadataReadResult AssetDatabase::readMetadata(
        const std::filesystem::path& source_path) const {
    const auto file = metadataPath(source_path);
    if (!file) {
        return { .error = "Metadata source path is invalid." };
    }

    std::ifstream input{ *file, std::ios::binary };
    if (!input.is_open()) {
        return { .error = "Failed to open metadata file." };
    }
    const std::string text{ std::istreambuf_iterator<char>{ input },
        std::istreambuf_iterator<char>{} };
    if (!input.good() && !input.eof()) {
        return { .error = "Failed while reading metadata file." };
    }

    std::string parse_error;
    auto metadata = deserializeMetadata(text, parse_error);
    if (!metadata) {
        return { .error = std::move(parse_error) };
    }
    if (metadata->source_path.lexically_normal() != source_path.lexically_normal()) {
        return { .error = "Metadata SourcePath does not match its .meta location." };
    }
    return { .metadata = std::move(metadata) };
}

bool AssetDatabase::writeMetadata(const AssetMetadata& metadata, std::string& error) const {
    const auto file = metadataPath(metadata.source_path);
    if (!file) {
        error = "Metadata source path is invalid.";
        return false;
    }

    auto text = serializeMetadata(metadata, error);
    if (!text) {
        return false;
    }

    std::error_code filesystem_error;
    std::filesystem::create_directories(file->parent_path(), filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }

    std::filesystem::path temporary = *file;
    temporary += ".tmp." + core::UUID::generate().toString();
    {
        std::ofstream output{ temporary, std::ios::binary | std::ios::trunc };
        if (!output.is_open()) {
            error = "Failed to open temporary metadata file for writing.";
            return false;
        }
        output << *text;
        output.flush();
        if (!output.good()) {
            error = "Failed while writing temporary metadata file.";
            output.close();
            std::filesystem::remove(temporary, filesystem_error);
            return false;
        }
    }

    if (!replaceFile(temporary, *file, error)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

bool AssetDatabase::canStore(const AssetMetadata& metadata) const {
    const auto source = m_source_paths.find(pathKey(metadata.source_path));
    if (source != m_source_paths.end() && source->second != metadata.handle) {
        return false;
    }
    const auto artifact = m_artifact_paths.find(pathKey(metadata.artifact_path));
    return artifact == m_artifact_paths.end() || artifact->second == metadata.handle;
}

void AssetDatabase::store(AssetMetadata metadata) {
    const core::UUID handle = metadata.handle;
    if (const auto old = m_metadata.find(handle); old != m_metadata.end()) {
        m_source_paths.erase(pathKey(old->second.source_path));
        m_artifact_paths.erase(pathKey(old->second.artifact_path));
    }

    const std::string source_key = pathKey(metadata.source_path);
    const std::string artifact_key = pathKey(metadata.artifact_path);
    m_metadata.insert_or_assign(handle, std::move(metadata));
    m_source_paths.insert_or_assign(source_key, handle);
    m_artifact_paths.insert_or_assign(artifact_key, handle);
}

std::optional<std::filesystem::path> AssetDatabase::metadataPath(
        const std::filesystem::path& source_path) const {
    auto path = resolveSourcePath(source_path);
    if (path) {
        *path += ".meta";
    }
    return path;
}

std::string AssetDatabase::pathKey(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

} // namespace arti::asset
