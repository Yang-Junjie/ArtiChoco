add_library(artichoco_renderer STATIC stub.cpp)
add_library(ArtiChoco::Renderer ALIAS artichoco_renderer)
artichoco_configure_library(artichoco_renderer)
target_link_libraries(artichoco_renderer
    PRIVATE
        ArtiSDK::GLM
        ArtiSDK::Slang
        ArtiSDK::Vulkan
)
