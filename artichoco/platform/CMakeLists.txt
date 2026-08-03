add_library(artichoco_platform STATIC
    platform_log.cpp
    window/sdl_platform.cpp
    window/sdl_vulkan_surface_source.cpp
    window/sdl_window.cpp
    window/window_factory.cpp
)
add_library(ArtiChoco::Platform ALIAS artichoco_platform)
artichoco_configure_library(artichoco_platform)
target_link_libraries(artichoco_platform
    PUBLIC
        ArtiChoco::Core
        ArtiChoco::Renderer
    PRIVATE
        ArtiSDK::SDL3
)
