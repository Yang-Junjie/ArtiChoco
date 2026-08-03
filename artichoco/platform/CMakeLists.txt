add_library(artichoco_platform STATIC
    platform_log.cpp
    window/sdl_platform.cpp
    window/sdl_window.cpp
    window/window_factory.cpp
)
add_library(ArtiChoco::Platform ALIAS artichoco_platform)
artichoco_configure_library(artichoco_platform)
target_link_libraries(artichoco_platform
    PUBLIC
        ArtiChoco::Core
    PRIVATE
        ArtiSDK::SDL3
        ArtiSDK::Vulkan
)
