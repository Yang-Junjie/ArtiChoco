add_library(artichoco_renderer STATIC
    detail/nvrhi_texture_readback.cpp
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
    # nvrhi_vk 单列在最前面：**静态库在链接行上的顺序是有意义的**（GNU ld 单遍扫描，被引用的
    # 库必须排在使用者后面），而 nvrhi_vk 的 .o 引用 nvrhi 里的符号。CMake 按这里的书写顺序
    # 生成链接行，所以「PRIVATE 一条 + 后面再 PUBLIC 一批」这种不常见的写法是刻意的。
    # MSVC 不在乎顺序 —— 但顺序有意义，别合并回一组。
    PRIVATE
        nvrhi_vk
    PUBLIC
        ArtiChoco::Core
        ArtiSDK::Vulkan
        # PUBLIC 而不是 PRIVATE：render_pass.h 等公开头里 include 了 nvrhi/nvrhi.h，
        # 下游要拿到它的 include 目录。
        nvrhi
    PRIVATE
        ArtiSDK::GLM
        ArtiSDK::Slang
)
