#include "artichoco/project/project_manager.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void runSmokeTest() {
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "artichoco_project_smoke";
    std::error_code error;
    fs::remove_all(root, error);

    arti::project::ProjectInfo info;
    info.name = "path_contract";
    info.version = "1.2.3";
    info.assets_path = "Assets";
    info.artifacts_path = "Library/Artifacts";
    info.start_scene = "Assets/Main.scene";

    auto& manager = arti::project::ProjectManager::instance();
    require(manager.createProject(root, info), "ProjectManager failed to create a project.");

    const fs::path absolute_root = fs::absolute(root).lexically_normal();
    require(manager.getProjectRootPath() == absolute_root,
            "ProjectManager returned an incorrect project root.");
    require(manager.getAssetsRootPath() == absolute_root / "Assets",
            "ProjectManager returned an incorrect assets root.");
    require(manager.getArtifactsRootPath() == absolute_root / "Library/Artifacts",
            "ProjectManager returned an incorrect artifacts root.");
    require(fs::is_directory(absolute_root / "Assets") &&
                    fs::is_directory(absolute_root / "Library/Artifacts"),
            "ProjectManager did not create both project data roots.");

    require(manager.resolveAssetPath("Textures/brick.png") ==
                    absolute_root / "Assets/Textures/brick.png",
            "Asset path resolution used the wrong root.");
    require(manager.resolveArtifactPath("texture.bin") ==
                    absolute_root / "Library/Artifacts/texture.bin",
            "Artifact path resolution used the wrong root.");
    require(!manager.resolveAssetPath("../outside.asset") &&
                    !manager.resolveArtifactPath(absolute_root / "outside.asset"),
            "Project path resolution accepted a path outside its configured root.");

    const auto old_root = manager.getProjectRootPath();
    const auto old_info = manager.getProjectInfo();
    const fs::path invalid_project = absolute_root / "invalid.artiproj";
    {
        std::ofstream output{ invalid_project };
        output << "Project:\n"
                  "  Name: invalid\n"
                  "  AssetsPath: ../outside\n"
                  "  ArtifactsPath: Library/Artifacts\n";
    }
    require(!manager.loadProject(invalid_project),
            "ProjectManager accepted a project with an escaping assets path.");
    require(manager.getProjectRootPath() == old_root && manager.getProjectInfo() == old_info,
            "A failed project load changed the active project state.");

    require(manager.loadProject(absolute_root / "path_contract.artiproj"),
            "ProjectManager failed to reload its own project file.");
    require(manager.getProjectInfo()->artifacts_path == "Library/Artifacts",
            "ProjectManager did not persist ArtifactsPath.");

    fs::remove_all(root, error);
}

} // namespace

int main() {
    try {
        runSmokeTest();
        std::cout << "Project path smoke test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Project path smoke test failed: " << exception.what() << '\n';
        return 1;
    }
}
