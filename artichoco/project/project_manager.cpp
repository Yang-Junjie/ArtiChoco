#include "project_manager.h"
#include "project_log.h"

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <yaml-cpp/yaml.h>

namespace arti::project {

bool ProjectManager::loadProject(const std::filesystem::path& project_file_path) {
    if (project_file_path.empty()) {
        getLogChannel().error("Failed to load project: project file path is empty");
        return false;
    }

    std::error_code error;
    const std::filesystem::path absolute_path =
            std::filesystem::absolute(project_file_path, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve project path '{}': {}", project_file_path.string(),
                error.message());
        return false;
    }

    const auto previous_info = m_project_info;
    const auto previous_root = m_project_root_path;
    const auto previous_file = m_project_file_path;
    m_project_file_path = absolute_path;
    m_project_root_path = absolute_path.parent_path();
    getLogChannel().info("Loading project from '{}'", m_project_file_path.value().string());
    if (deserializeProject()) {
        return true;
    }

    m_project_info = previous_info;
    m_project_root_path = previous_root;
    m_project_file_path = previous_file;
    return false;
}

std::optional<std::filesystem::path> ProjectManager::getProjectRootPath() const {
    return m_project_root_path;
}

std::optional<std::filesystem::path> ProjectManager::getProjectFilePath() const {
    return m_project_file_path;
}

std::optional<std::filesystem::path> ProjectManager::getAssetsRootPath() const {
    if (!m_project_root_path || !m_project_info) {
        return std::nullopt;
    }
    return (m_project_root_path.value() / m_project_info->assets_path).lexically_normal();
}

std::optional<std::filesystem::path> ProjectManager::getArtifactsRootPath() const {
    if (!m_project_root_path || !m_project_info) {
        return std::nullopt;
    }
    return (m_project_root_path.value() / m_project_info->artifacts_path).lexically_normal();
}

std::optional<std::filesystem::path> ProjectManager::resolveAssetPath(
        const std::filesystem::path& relative_path) const {
    if (!isSafeRelativePath(relative_path, false)) {
        return std::nullopt;
    }
    const auto root = getAssetsRootPath();
    if (!root) {
        return std::nullopt;
    }
    return (root.value() / relative_path.lexically_normal()).lexically_normal();
}

std::optional<std::filesystem::path> ProjectManager::resolveArtifactPath(
        const std::filesystem::path& relative_path) const {
    if (!isSafeRelativePath(relative_path, false)) {
        return std::nullopt;
    }
    const auto root = getArtifactsRootPath();
    if (!root) {
        return std::nullopt;
    }
    return (root.value() / relative_path.lexically_normal()).lexically_normal();
}

const std::optional<ProjectInfo>& ProjectManager::getProjectInfo() const { return m_project_info; }

void ProjectManager::setProjectInfo(const ProjectInfo& info) {
    if (!isValidProjectInfo(info)) {
        throw std::invalid_argument("The ProjectInfo contains an invalid path or project name.");
    }
    m_project_info = info;
}

bool ProjectManager::saveProject() { return serializeProject(); }

bool ProjectManager::createProject(const std::filesystem::path& project_root_path,
        const ProjectInfo& info) {
    if (!isValidProjectInfo(info)) {
        getLogChannel().error("Failed to create project: ProjectInfo is invalid");
        return false;
    }

    if (project_root_path.empty()) {
        getLogChannel().error("Failed to create project: project root path is empty");
        return false;
    }

    std::error_code error;
    const std::filesystem::path absolute_root =
            std::filesystem::absolute(project_root_path, error).lexically_normal();
    if (error) {
        getLogChannel().error("Failed to resolve project root '{}': {}", project_root_path.string(),
                error.message());
        return false;
    }

    const auto previous_info = m_project_info;
    const auto previous_root = m_project_root_path;
    const auto previous_file = m_project_file_path;
    m_project_root_path = absolute_root;
    m_project_file_path = *m_project_root_path / (info.name + ".artiproj");
    m_project_info = info;
    getLogChannel().info("Creating project '{}' at '{}'", info.name,
            m_project_root_path.value().string());

    std::filesystem::create_directories(*m_project_root_path, error);
    if (error) {
        getLogChannel().error("Failed to create project directory '{}': {}",
                m_project_root_path.value().string(), error.message());
        m_project_info = previous_info;
        m_project_root_path = previous_root;
        m_project_file_path = previous_file;
        return false;
    }

    error.clear();
    std::filesystem::create_directories(*m_project_root_path / info.assets_path, error);
    if (error) {
        getLogChannel().error("Failed to create assets directory '{}': {}",
                (*m_project_root_path / info.assets_path).string(), error.message());
        m_project_info = previous_info;
        m_project_root_path = previous_root;
        m_project_file_path = previous_file;
        return false;
    }

    error.clear();
    std::filesystem::create_directories(*m_project_root_path / info.artifacts_path, error);
    if (error) {
        getLogChannel().error("Failed to create artifacts directory '{}': {}",
                (*m_project_root_path / info.artifacts_path).string(), error.message());
        m_project_info = previous_info;
        m_project_root_path = previous_root;
        m_project_file_path = previous_file;
        return false;
    }

    if (serializeProject()) {
        return true;
    }

    m_project_info = previous_info;
    m_project_root_path = previous_root;
    m_project_file_path = previous_file;
    return false;
}

bool ProjectManager::serializeProject() {
    if (!m_project_info || !m_project_file_path) {
        getLogChannel().error("Failed to serialize project: no project is loaded");
        return false;
    }

    YAML::Node root;
    YAML::Node project;
    project["Name"] = m_project_info.value().name;
    project["Version"] = m_project_info.value().version;
    project["Author"] = m_project_info.value().author;
    project["Description"] = m_project_info.value().description;
    project["StartScene"] = m_project_info.value().start_scene.generic_string();
    project["LastOpenScene"] = m_project_info.value().last_open_scene.generic_string();
    project["AssetsPath"] = m_project_info.value().assets_path.generic_string();
    project["ArtifactsPath"] = m_project_info.value().artifacts_path.generic_string();
    root["Project"] = project;

    std::ofstream out(*m_project_file_path);
    if (!out.is_open()) {
        getLogChannel().error("Failed to open project file for writing: '{}'",
                m_project_file_path.value().string());
        return false;
    }

    out << root;
    if (!out.good()) {
        getLogChannel().error("Failed to write project file: '{}'",
                m_project_file_path.value().string());
        return false;
    }

    getLogChannel().info("Saved project '{}' to '{}'", m_project_info.value().name,
            m_project_file_path.value().string());
    return true;
}

bool ProjectManager::deserializeProject() {
    if (!m_project_file_path) {
        getLogChannel().error("Failed to deserialize project: project file path is empty");
        return false;
    }

    try {
        const YAML::Node root = YAML::LoadFile(m_project_file_path.value().string());
        const YAML::Node project = root["Project"];
        if (!project || !project["Name"]) {
            getLogChannel().error("Failed to deserialize project '{}': missing Project/Name",
                    m_project_file_path.value().string());
            return false;
        }

        ProjectInfo info;
        info.name = project["Name"].as<std::string>();
        if (project["Version"]) {
            info.version = project["Version"].as<std::string>();
        }
        if (project["Author"]) {
            info.author = project["Author"].as<std::string>();
        }
        if (project["Description"]) {
            info.description = project["Description"].as<std::string>();
        }
        if (project["StartScene"]) {
            info.start_scene = project["StartScene"].as<std::string>();
        }
        if (project["LastOpenScene"]) {
            info.last_open_scene = project["LastOpenScene"].as<std::string>();
        }
        if (project["AssetsPath"]) {
            info.assets_path = project["AssetsPath"].as<std::string>();
        }
        if (project["ArtifactsPath"]) {
            info.artifacts_path = project["ArtifactsPath"].as<std::string>();
        }
        if (!isValidProjectInfo(info)) {
            getLogChannel().error("Failed to deserialize project '{}': ProjectInfo is invalid",
                    m_project_file_path.value().string());
            return false;
        }

        m_project_info = info;
        getLogChannel().info("Loaded project '{}' from '{}' (assets: '{}', artifacts: '{}', start "
                             "scene: '{}', last open scene: '{}')",
                m_project_info.value().name, m_project_file_path.value().string(),
                m_project_info.value().assets_path.string(),
                m_project_info.value().artifacts_path.string(),
                m_project_info.value().start_scene.string(),
                m_project_info.value().last_open_scene.string());
        return true;
    } catch (const YAML::Exception& error) {
        getLogChannel().error("Failed to deserialize project '{}': {}",
                m_project_file_path.value().string(), error.what());
        return false;
    } catch (const std::filesystem::filesystem_error& error) {
        getLogChannel().error("Failed to deserialize project '{}': {}",
                m_project_file_path.value().string(), error.what());
        return false;
    }
}

bool ProjectManager::isSafeRelativePath(const std::filesystem::path& path, bool allow_empty) {
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
    for (const auto& component: normalized) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool ProjectManager::isValidProjectName(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    const std::filesystem::path path{ name };
    return !path.is_absolute() && !path.has_root_name() && !path.has_root_directory() &&
           !path.has_parent_path() && name != "." && name != "..";
}

bool ProjectManager::isValidProjectInfo(const ProjectInfo& info) {
    return isValidProjectName(info.name) && isSafeRelativePath(info.assets_path, false) &&
           isSafeRelativePath(info.artifacts_path, false) &&
           isSafeRelativePath(info.start_scene, true) &&
           isSafeRelativePath(info.last_open_scene, true);
}

} // namespace arti::project
