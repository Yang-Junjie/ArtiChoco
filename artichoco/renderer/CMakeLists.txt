add_library(artichoco_renderer STATIC
    index_buffer.cpp
    render_pass.cpp
    renderer_log.cpp
    slang_compiler.cpp
    texture_2d.cpp
    texture_cube.cpp
    vertex_buffer.cpp
    vulkan/vulkan_context.cpp
    vulkan/vulkan_device.cpp
    vulkan/vulkan_frame_manager.cpp
    vulkan/nvrhi_vulkan_dispatch.cpp
    vulkan/nvrhi_vulkan_device.cpp
    vulkan/nvrhi_resource_smoke.cpp
    vulkan/nvrhi_resource_upload.cpp
    vulkan/nvrhi_mipmap.cpp
    vulkan/nvrhi_shader_factory.cpp
    render_device.cpp
    vulkan/vulkan_surface.cpp
    vulkan/vulkan_swapchain.cpp
)
add_library(ArtiChoco::Renderer ALIAS artichoco_renderer)
artichoco_configure_library(artichoco_renderer)
if(WIN32)
    target_compile_definitions(artichoco_renderer PRIVATE NOMINMAX)
endif()
target_link_libraries(artichoco_renderer
    PUBLIC
        ArtiChoco::Core
        ArtiSDK::Vulkan
        nvrhi
    PRIVATE
        ArtiSDK::GLM
        ArtiSDK::Slang
        nvrhi_vk
)
