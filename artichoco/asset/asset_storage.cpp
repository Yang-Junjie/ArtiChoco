#include "asset_storage.h"

#include "asset_log.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace arti::asset {
namespace {

bool readFile(const std::filesystem::path& file, std::string& contents) {
    std::ifstream input{ file, std::ios::binary };
    if (!input.is_open()) {
        return false;
    }
    contents.assign(std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{});
    return input.good() || input.eof();
}

bool writeFile(const std::filesystem::path& file, const std::string& contents) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output{ file, std::ios::binary | std::ios::trunc };
    if (!output.is_open()) {
        return false;
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return output.good();
}

}

bool AssetStorage::open(std::filesystem::path assets_root,
        std::filesystem::path artifacts_root) {
    close();
    if (assets_root.empty() || artifacts_root.empty()) {
        getLogChannel().error("Failed to open AssetStorage: a workspace root path is empty");
        return false;
    }

    std::error_code error;
    assets_root = std::filesystem::absolute(assets_root, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve the Assets root: {}", error.message());
        return false;
    }
    artifacts_root = std::filesystem::absolute(artifacts_root, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve the Artifacts root: {}", error.message());
        return false;
    }

    std::filesystem::create_directories(assets_root, error);
    if (error) {
        getLogChannel().error("Failed to create the Assets root '{}': {}", assets_root.string(),
                error.message());
        return false;
    }
    std::filesystem::create_directories(artifacts_root, error);
    if (error) {
        getLogChannel().error("Failed to create the Artifacts root '{}': {}",
                artifacts_root.string(), error.message());
        return false;
    }

    m_assets_root = std::move(assets_root);
    m_artifacts_root = std::move(artifacts_root);
    getLogChannel().info("Opened AssetStorage (sources '{}', artifacts '{}')",
            m_assets_root.string(), m_artifacts_root.string());
    return true;
}

void AssetStorage::close() noexcept {
    m_assets_root.clear();
    m_artifacts_root.clear();
}

std::optional<std::vector<AssetMetadata>> AssetStorage::scanMetadata() const {
    if (!isOpen()) {
        getLogChannel().warn("Cannot scan Asset metadata: AssetStorage is not open");
        return std::nullopt;
    }

    std::vector<AssetMetadata> metadata;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator{ m_assets_root,
        std::filesystem::directory_options::skip_permission_denied, error };
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        if (iterator->is_regular_file(error)) {
            const std::string relative_name =
                    std::filesystem::relative(iterator->path(), m_assets_root, error)
                            .generic_string();
            if (!error && relative_name.ends_with(kAssetMetadataExtension)) {
                const std::filesystem::path source_path{ relative_name.substr(
                        0, relative_name.size() - kAssetMetadataExtension.size()) };

                std::string text;
                if (!readFile(iterator->path(), text)) {
                    getLogChannel().error("Failed to read Asset metadata '{}'",
                            iterator->path().string());
                    return std::nullopt;
                }
                const auto entry = deserializeAssetMetadata(text);
                if (!entry ||
                        entry->source_path.lexically_normal() != source_path.lexically_normal()) {
                    getLogChannel().error("Invalid or misplaced Asset metadata '{}'",
                            iterator->path().string());
                    return std::nullopt;
                }
                metadata.push_back(*entry);
            }
        }
        iterator.increment(error);
    }
    if (error) {
        getLogChannel().error("Failed while scanning the Assets root '{}': {}",
                m_assets_root.string(), error.message());
        return std::nullopt;
    }
    return metadata;
}

bool AssetStorage::writeMetadata(const AssetMetadata& metadata) {
    if (!isOpen()) {
        getLogChannel().warn("Cannot write Asset metadata: AssetStorage is not open");
        return false;
    }

    auto meta_file = resolveSourcePath(metadata.source_path);
    const auto text = serializeAssetMetadata(metadata);
    if (!meta_file || !text) {
        getLogChannel().error("Failed to serialize Asset metadata for '{}'",
                metadata.source_path.string());
        return false;
    }
    *meta_file += kAssetMetadataExtension;
    if (!writeFile(*meta_file, *text)) {
        getLogChannel().error("Failed to write Asset metadata '{}'", meta_file->string());
        return false;
    }
    return true;
}

bool AssetStorage::writeArtifact(const std::filesystem::path& relative_path,
        const std::vector<std::byte>& data) {
    if (!isOpen()) {
        getLogChannel().warn("Cannot write an Artifact: AssetStorage is not open");
        return false;
    }
    if (!isSafeAssetRelativePath(relative_path)) {
        getLogChannel().error("Cannot write an Artifact: unsafe relative path '{}'",
                relative_path.string());
        return false;
    }
    const auto file = resolveArtifactPath(relative_path);
    if (!file) {
        getLogChannel().error("Failed to resolve the Artifact path '{}'", relative_path.string());
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(file->parent_path(), error);
    if (error) {
        getLogChannel().error("Failed to create '{}': {}", file->parent_path().string(),
                error.message());
        return false;
    }
    std::ofstream output{ *file, std::ios::binary | std::ios::trunc };
    if (!output.is_open()) {
        getLogChannel().error("Failed to open '{}' for writing", file->string());
        return false;
    }
    output.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    if (!output.good()) {
        getLogChannel().error("Failed while writing '{}'", file->string());
        return false;
    }
    return true;
}

std::optional<std::filesystem::path> AssetStorage::resolveSourcePath(
        const std::filesystem::path& relative_path) const {
    if (!isOpen() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_assets_root / relative_path.lexically_normal()).lexically_normal();
}

std::optional<std::filesystem::path> AssetStorage::resolveArtifactPath(
        const std::filesystem::path& relative_path) const {
    if (!isOpen() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_artifacts_root / relative_path.lexically_normal()).lexically_normal();
}

}
