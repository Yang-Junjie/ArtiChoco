#pragma once
#include <filesystem>
#include <string>

namespace arti::project {

struct ProjectInfo {
    std::string name{ "Unknown" };
    std::string version{ "0.1.0" };
    std::string author{ "Unknown" };
    std::string description{ "An ArtiChoco project." };

    // Scene paths are persisted relative to the project root.
    std::filesystem::path start_scene;
    std::filesystem::path last_open_scene;

    // Source assets and generated artifacts intentionally use different roots.
    // Both roots are persisted relative to the project root.
    std::filesystem::path assets_path{ "Assets" };
    std::filesystem::path artifacts_path{ "Library/Artifacts" };

    bool operator==(const ProjectInfo&) const = default;
};

} // namespace arti::project
