add_executable(render_to_texture
    application.cpp
    display_pass.cpp
    render_texture_pass.cpp
    render_to_texture_layer.cpp
)

set(_arti_render_to_texture_shader_directory
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
set(_arti_render_to_texture_source_shader
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/render_texture.slang"
)
set(_arti_render_to_texture_source_shader_output
    "${_arti_render_to_texture_shader_directory}/render_texture.slang"
)
set(_arti_render_to_texture_display_shader
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/display.slang"
)
set(_arti_render_to_texture_display_shader_output
    "${_arti_render_to_texture_shader_directory}/display.slang"
)

add_custom_command(
    OUTPUT "${_arti_render_to_texture_source_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_render_to_texture_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_render_to_texture_source_shader}"
        "${_arti_render_to_texture_source_shader_output}"
    DEPENDS "${_arti_render_to_texture_source_shader}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_arti_render_to_texture_display_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_render_to_texture_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_render_to_texture_display_shader}"
        "${_arti_render_to_texture_display_shader_output}"
    DEPENDS "${_arti_render_to_texture_display_shader}"
    VERBATIM
)
add_custom_target(render_to_texture_assets
    DEPENDS
        "${_arti_render_to_texture_source_shader_output}"
        "${_arti_render_to_texture_display_shader_output}"
)
add_dependencies(render_to_texture render_to_texture_assets)

target_compile_definitions(render_to_texture PRIVATE
    ARTI_RENDER_TO_TEXTURE_SHADER_PATH="${_arti_render_to_texture_source_shader_output}"
    ARTI_RENDER_TO_TEXTURE_DISPLAY_SHADER_PATH="${_arti_render_to_texture_display_shader_output}"
)
target_include_directories(render_to_texture PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(render_to_texture
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(render_to_texture)
set_target_properties(render_to_texture PROPERTIES FOLDER "Examples/Graphics")
