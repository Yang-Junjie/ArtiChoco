#pragma once

#include "asset_importer.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arti::asset {

// AssetDatabase is synchronous. Keep access on one owning thread until an
// explicit Asset-thread model is introduced.
class AssetDatabase final {
public:
    [[nodiscard]] bool open(std::filesystem::path assets_root,
            std::filesystem::path artifacts_root);
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return !m_assets_root.empty(); }
    [[nodiscard]] bool registerImporter(std::unique_ptr<AssetImporter> importer);

    [[nodiscard]] AssetImportResult import(const std::filesystem::path& source_path);

    [[nodiscard]] std::optional<AssetMetadata> find(core::UUID handle) const;
    [[nodiscard]] std::optional<AssetMetadata> findBySourcePath(
            const std::filesystem::path& source_path) const;

    [[nodiscard]] std::optional<std::filesystem::path> resolveSourcePath(
            const std::filesystem::path& relative_path) const;
    [[nodiscard]] std::optional<std::filesystem::path> resolveArtifactPath(
            const std::filesystem::path& relative_path) const;

    [[nodiscard]] const std::filesystem::path& getAssetsRoot() const noexcept {
        return m_assets_root;
    }
    [[nodiscard]] const std::filesystem::path& getArtifactsRoot() const noexcept {
        return m_artifacts_root;
    }
    [[nodiscard]] size_t size() const noexcept { return m_metadata.size(); }
    [[nodiscard]] bool empty() const noexcept { return m_metadata.empty(); }

private:
    struct ImporterEntry {
        std::unique_ptr<AssetImporter> importer;
        std::vector<std::string> extensions;
    };

    struct MetadataReadResult {
        std::optional<AssetMetadata> metadata;
        std::string error;
    };

    [[nodiscard]] AssetImporter* findImporter(const std::filesystem::path& source_path);
    [[nodiscard]] MetadataReadResult readMetadata(const std::filesystem::path& source_path) const;
    [[nodiscard]] bool writeMetadata(const AssetMetadata& metadata, std::string& error) const;
    [[nodiscard]] bool canStore(const AssetMetadata& metadata) const;
    void store(AssetMetadata metadata);

    [[nodiscard]] std::optional<std::filesystem::path> metadataPath(
            const std::filesystem::path& source_path) const;
    [[nodiscard]] static std::string pathKey(const std::filesystem::path& path);

    std::filesystem::path m_assets_root;
    std::filesystem::path m_artifacts_root;
    std::unordered_map<core::UUID, AssetMetadata> m_metadata;
    std::unordered_map<std::string, core::UUID> m_source_paths;
    std::unordered_map<std::string, core::UUID> m_artifact_paths;
    std::vector<ImporterEntry> m_importers;
};

} // namespace arti::asset
