#pragma once
#include "project_info.h"

#include <filesystem>
#include <optional>

namespace arti::project {

class ProjectManager {
public:
    bool loadProject(const std::filesystem::path& project_file_path);

    std::optional<std::filesystem::path> getProjectRootPath() const;

    const std::optional<ProjectInfo>& getProjectInfo() const;

    void setProjectInfo(const ProjectInfo& info);

    bool saveProject();

    bool createProject(const std::filesystem::path& project_root_path, const ProjectInfo& info);

    static ProjectManager& instance()
    {
        static ProjectManager manager;
        return manager;
    }

private:
    bool serializeProject();
    bool deserializeProject();

    std::optional<ProjectInfo> m_project_info{std::nullopt};
    std::optional<std::filesystem::path> m_project_root_path{std::nullopt};
    std::optional<std::filesystem::path> m_project_file_path{std::nullopt};
};

} // namespace arti::project
