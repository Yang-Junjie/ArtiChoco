add_library(artichoco_renderer STATIC
    vulkan/frame_resources.cpp
    renderer_log.cpp
    slang_compiler.cpp
    vulkan/vulkan_allocator.cpp
    vulkan/vulkan_context.cpp
    vulkan/vulkan_device.cpp
    vulkan/vulkan_pipeline.cpp
    vulkan/vulkan_shader.cpp
    renderer.cpp
    vulkan/vulkan_surface.cpp
    vulkan/vulkan_swapchain.cpp
)
add_library(ArtiChoco::Renderer ALIAS artichoco_renderer)
artichoco_configure_library(artichoco_renderer)
target_link_libraries(artichoco_renderer
    PUBLIC
        ArtiChoco::Core
        ArtiSDK::Vulkan
        GPUOpen::VulkanMemoryAllocator
    PRIVATE
        ArtiSDK::GLM
        ArtiSDK::Slang
)
