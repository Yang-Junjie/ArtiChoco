add_executable(renderer_showcase
    application.cpp
    image_loader.cpp
    renderer_showcase_layer.cpp
    render_system.cpp
    passes/alpha_blending/alpha_blending_pass.cpp
    passes/common/image_display_pass.cpp
    passes/common/indexed_graphics_pass.cpp
    passes/compute_texture/compute_texture_pass.cpp
    passes/cubemap/cubemap_pass.cpp
    passes/depth_test/depth_test_pass.cpp
    passes/instancing/instancing_pass.cpp
    passes/offscreen/offscreen_pass.cpp
    passes/push_constants/push_constants_pass.cpp
    passes/render_to_cubemap/render_to_cubemap_pass.cpp
    passes/textured_quad/textured_quad_pass.cpp
    passes/triangle/triangle_pass.cpp
    passes/uniform_buffer/uniform_buffer_pass.cpp
)

set(_arti_renderer_showcase_shader_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(_arti_renderer_showcase_shader_names
    alpha_blending.slang
    compute_texture.slang
    cubemap.slang
    depth_test.slang
    image_display.slang
    instancing.slang
    offscreen.slang
    push_constants.slang
    textured_quad.slang
    triangle.slang
    uniform_buffer.slang
)
set(_arti_renderer_showcase_shader_outputs)
foreach(_shader_name IN LISTS _arti_renderer_showcase_shader_names)
    set(_shader_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_shader_name}")
    set(_shader_output "${_arti_renderer_showcase_shader_directory}/${_shader_name}")
    add_custom_command(
        OUTPUT "${_shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_renderer_showcase_shader_directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_shader_source}" "${_shader_output}"
        DEPENDS "${_shader_source}"
        VERBATIM
    )
    list(APPEND _arti_renderer_showcase_shader_outputs "${_shader_output}")
endforeach()

set(_arti_renderer_showcase_brownie_texture "${CMAKE_CURRENT_SOURCE_DIR}/assets/brownie.png")
set(_arti_renderer_showcase_asset_directory "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_arti_renderer_showcase_brownie_texture_output
    "${_arti_renderer_showcase_asset_directory}/brownie.png"
)
add_custom_command(
    OUTPUT "${_arti_renderer_showcase_brownie_texture_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_renderer_showcase_asset_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_renderer_showcase_brownie_texture}"
        "${_arti_renderer_showcase_brownie_texture_output}"
    DEPENDS "${_arti_renderer_showcase_brownie_texture}"
    VERBATIM
)
add_custom_target(renderer_showcase_assets
    DEPENDS
        ${_arti_renderer_showcase_shader_outputs}
        "${_arti_renderer_showcase_brownie_texture_output}"
)
add_dependencies(renderer_showcase renderer_showcase_assets)

target_compile_definitions(renderer_showcase PRIVATE
    ARTI_RENDERER_SHOWCASE_SHADER_DIRECTORY="${_arti_renderer_showcase_shader_directory}"
    ARTI_RENDERER_SHOWCASE_BROWNIE_TEXTURE_PATH="${_arti_renderer_showcase_brownie_texture_output}"
)
target_include_directories(renderer_showcase SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb"
)
target_include_directories(renderer_showcase PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(renderer_showcase
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(renderer_showcase)
set_target_properties(renderer_showcase PROPERTIES FOLDER "Examples")
