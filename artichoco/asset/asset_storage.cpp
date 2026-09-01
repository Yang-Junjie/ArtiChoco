#include "asset_storage.h"

#include "asset_log.h"

#include <chrono>
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

bool AssetStorage::openArtifactsOnly(std::filesystem::path artifacts_root) {
    close();
    if (artifacts_root.empty()) {
        getLogChannel().error("Failed to open AssetStorage: the Artifacts root path is empty");
        return false;
    }

    std::error_code error;
    artifacts_root = std::filesystem::absolute(artifacts_root, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve the Artifacts root: {}", error.message());
        return false;
    }
    if (!std::filesystem::is_directory(artifacts_root, error) || error) {
        getLogChannel().error("The Artifacts root '{}' does not exist", artifacts_root.string());
        return false;
    }

    m_artifacts_root = std::move(artifacts_root);
    getLogChannel().info("Opened AssetStorage in packaged mode (artifacts '{}')",
            m_artifacts_root.string());
    return true;
}

void AssetStorage::close() noexcept {
    m_assets_root.clear();
    m_artifacts_root.clear();
}

MetadataScan AssetStorage::scanMetadata() const {
    MetadataScan scan;
    if (!hasSources()) {
        scan.traversal_error = "AssetStorage has no source tree";
        getLogChannel().warn("Cannot scan Asset metadata: AssetStorage has no source tree");
        return scan;
    }

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator{ m_assets_root,
        std::filesystem::directory_options::skip_permission_denied, error };
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        if (iterator->is_regular_file(error)) {
            const std::filesystem::path relative =
                    std::filesystem::relative(iterator->path(), m_assets_root, error);
            const std::string relative_name = relative.generic_string();
            if (!error && relative_name.ends_with(kAssetMetadataExtension)) {
                const std::filesystem::path source_path{ relative_name.substr(
                        0, relative_name.size() - kAssetMetadataExtension.size()) };

                std::string text;
                if (!readFile(iterator->path(), text)) {
                    scan.issues.push_back({ relative, MetadataIssueKind::Unreadable,
                        "the file could not be read" });
                } else if (const auto entry = deserializeSourceMetadata(text); !entry) {
                    scan.issues.push_back({ relative, MetadataIssueKind::Malformed,
                        "the metadata failed to parse or validate" });
                } else if (entry->source_path.lexically_normal() !=
                           source_path.lexically_normal()) {
                    scan.issues.push_back({ relative, MetadataIssueKind::Misplaced,
                        "SourcePath is '" + entry->source_path.generic_string() +
                                "' but the sidecar sits next to '" + source_path.generic_string() +
                                "'" });
                } else {
                    scan.entries.push_back(*entry);
                }
            }
        }
        iterator.increment(error);
    }
    if (error) {
        scan.traversal_error = error.message();
        getLogChannel().error("Failed while scanning the Assets root '{}': {}",
                m_assets_root.string(), error.message());
    }
    for (const MetadataIssue& issue: scan.issues) {
        getLogChannel().error("Rejected Asset metadata '{}': {}", issue.meta_file.generic_string(),
                issue.detail);
    }
    return scan;
}

SourceScan AssetStorage::scanSources() const {
    SourceScan scan;
    if (!hasSources()) {
        scan.traversal_error = "AssetStorage has no source tree";
        return scan;
    }

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator{ m_assets_root,
        std::filesystem::directory_options::skip_permission_denied, error };
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        if (iterator->is_regular_file(error)) {
            const std::filesystem::path relative =
                    std::filesystem::relative(iterator->path(), m_assets_root, error);
            if (!error && !relative.generic_string().ends_with(kAssetMetadataExtension)) {
                SourceFile file;
                file.relative_path = relative;

                std::error_code stat_error;
                const auto size = std::filesystem::file_size(iterator->path(), stat_error);
                if (!stat_error) {
                    file.size = size;
                }
                const auto written = std::filesystem::last_write_time(iterator->path(),
                        stat_error);
                if (!stat_error) {
                    file.modified_time = std::chrono::duration_cast<std::chrono::seconds>(
                            written.time_since_epoch())
                                                 .count();
                }
                scan.files.push_back(std::move(file));
            }
        }
        iterator.increment(error);
    }
    if (error) {
        scan.traversal_error = error.message();
    }
    return scan;
}

std::filesystem::path AssetStorage::metadataPathFor(const std::filesystem::path& source_path) {
    std::filesystem::path meta = source_path.lexically_normal();
    meta += kAssetMetadataExtension;
    return meta;
}

bool AssetStorage::writeMetadata(const SourceMetadata& metadata) {
    if (!isOpen()) {
        getLogChannel().warn("Cannot write Asset metadata: AssetStorage is not open");
        return false;
    }

    const auto meta_file = resolveSourcePath(metadataPathFor(metadata.source_path));
    const auto text = serializeSourceMetadata(metadata);
    if (!meta_file || !text) {
        getLogChannel().error("Failed to serialize Asset metadata for '{}'",
                metadata.source_path.string());
        return false;
    }
    if (!writeFile(*meta_file, *text)) {
        getLogChannel().error("Failed to write Asset metadata '{}'", meta_file->string());
        return false;
    }
    return true;
}

std::optional<SourceMetadata> AssetStorage::readMetadata(
        const std::filesystem::path& source_path) const {
    const auto meta_file = resolveSourcePath(metadataPathFor(source_path));
    if (!meta_file) {
        return std::nullopt;
    }
    std::string text;
    if (!readFile(*meta_file, text)) {
        return std::nullopt;
    }
    auto metadata = deserializeSourceMetadata(text);
    if (!metadata || metadata->source_path.lexically_normal() != source_path.lexically_normal()) {
        return std::nullopt;
    }
    return metadata;
}

bool AssetStorage::removeMetadata(const std::filesystem::path& source_path) {
    if (!isOpen()) {
        return false;
    }
    const auto meta_file = resolveSourcePath(metadataPathFor(source_path));
    if (!meta_file) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(*meta_file, error);
    if (error) {
        getLogChannel().error("Failed to remove Asset metadata '{}': {}", meta_file->string(),
                error.message());
        return false;
    }
    return true;
}

bool AssetStorage::removeArtifact(const std::filesystem::path& relative_path) {
    if (!isOpen()) {
        return false;
    }
    const auto file = resolveArtifactPath(relative_path);
    if (!file) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(*file, error);
    if (error) {
        getLogChannel().error("Failed to remove Artifact '{}': {}", file->string(),
                error.message());
        return false;
    }
    return true;
}

bool AssetStorage::hasSource(const std::filesystem::path& relative_path) const {
    const auto file = resolveSourcePath(relative_path);
    if (!file) {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(*file, error) && !error;
}

bool AssetStorage::hasArtifact(const std::filesystem::path& relative_path) const {
    const auto file = resolveArtifactPath(relative_path);
    if (!file) {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(*file, error) && !error;
}

std::optional<uint64_t> AssetStorage::sourceSize(const std::filesystem::path& relative_path) const {
    const auto file = resolveSourcePath(relative_path);
    if (!file) {
        return std::nullopt;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(*file, error);
    if (error) {
        return std::nullopt;
    }
    return size;
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
    // writeMetadata / readMetadata / removeMetadata / hasSource / sourceSize 全都从这里取路径，
    // 所以打包模式下只要在这一处失败，整条源文件侧的 API 就自动都失效了。
    if (!hasSources() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_assets_root / relative_path.lexically_normal()).lexically_normal();
}

std::optional<std::filesystem::path> AssetStorage::relativeSourcePath(
        const std::filesystem::path& absolute_path) const {
    if (!hasSources()) {
        return std::nullopt;
    }
    std::error_code error;
    const auto normalized = std::filesystem::absolute(absolute_path, error).lexically_normal();
    if (error) {
        return std::nullopt;
    }
    const auto relative = std::filesystem::relative(normalized, m_assets_root, error);
    if (error || relative.empty() || !isSafeAssetRelativePath(relative)) {
        return std::nullopt;
    }
    return relative;
}

std::optional<std::filesystem::path> AssetStorage::resolveArtifactPath(
        const std::filesystem::path& relative_path) const {
    if (!isOpen() || !isSafeAssetRelativePath(relative_path)) {
        return std::nullopt;
    }
    return (m_artifacts_root / relative_path.lexically_normal()).lexically_normal();
}

}
