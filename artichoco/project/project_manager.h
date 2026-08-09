#pragma once
#include "project_info.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace arti::project {

class ProjectManager {
public:
    bool loadProject(const std::filesystem::path& project_file_path);

    std::optional<std::filesystem::path> getProjectRootPath() const;
    std::optional<std::filesystem::path> getProjectFilePath() const;
    std::optional<std::filesystem::path> getAssetsRootPath() const;
    std::optional<std::filesystem::path> getArtifactsRootPath() const;

    std::optional<std::filesystem::path> resolveAssetPath(
            const std::filesystem::path& relative_path) const;
    std::optional<std::filesystem::path> resolveArtifactPath(
            const std::filesystem::path& relative_path) const;

    const std::optional<ProjectInfo>& getProjectInfo() const;

    void setProjectInfo(const ProjectInfo& info);

    bool saveProject();

    bool createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info);

    static ProjectManager& instance() {
        static ProjectManager manager;
        return manager;
    }

private:
    static bool isSafeRelativePath(const std::filesystem::path& path, bool allow_empty);
    static bool isValidProjectInfo(const ProjectInfo& info);
    static bool isValidProjectName(std::string_view name);

    bool serializeProject();
    bool deserializeProject();

    std::optional<ProjectInfo> m_project_info{ std::nullopt };
    std::optional<std::filesystem::path> m_project_root_path{ std::nullopt };
    std::optional<std::filesystem::path> m_project_file_path{ std::nullopt };
};

} // namespace arti::project
