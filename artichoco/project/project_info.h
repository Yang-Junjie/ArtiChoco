#pragma once
#include <filesystem>
#include <string>

namespace arti::project {

struct ProjectInfo {
    std::string name{"Unknown"};
    std::string version{"0.1.0"};
    std::string author{"Unknown"};
    std::string description{"An ArtiChoco project."};

    // relative to project root
    std::filesystem::path start_scene;
    std::filesystem::path last_open_scene;
    std::filesystem::path assets_path;
};

} // namespace arti::project
