add_library(artichoco_core STATIC
    application.cpp
    entry_point.cpp
    io/input.cpp
    io/paths.cpp
    layer_stack.cpp
    log.cpp
    task/task_system.cpp
    uuid.cpp
    window.cpp
)
add_library(ArtiChoco::Core ALIAS artichoco_core)

artichoco_configure_library(artichoco_core)
target_include_directories(artichoco_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/event"
        "${CMAKE_CURRENT_SOURCE_DIR}/io"
)
target_link_libraries(artichoco_core
    PUBLIC
        ArtiSDK::GLM
        spdlog::spdlog
        enkiTS
)
