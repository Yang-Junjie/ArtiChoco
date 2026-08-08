add_library(artichoco_project STATIC
    project_log.cpp
    project_manager.cpp
)
add_library(ArtiChoco::Project ALIAS artichoco_project)
artichoco_configure_library(artichoco_project)
target_link_libraries(artichoco_project
    PUBLIC
        ArtiChoco::Core
        yaml-cpp::yaml-cpp
)
