add_executable(basic_lighting
    application.cpp
    basic_lighting_layer.cpp
    lighting_pass.cpp
    present_pass.cpp
)

set(_arti_basic_lighting_shader_directory
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
set(_arti_basic_lighting_shader_source
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/lighting.slang"
)
set(_arti_basic_lighting_shader_output
    "${_arti_basic_lighting_shader_directory}/lighting.slang"
)
set(_arti_basic_lighting_present_shader_source
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/present.slang"
)
set(_arti_basic_lighting_present_shader_output
    "${_arti_basic_lighting_shader_directory}/present.slang"
)

add_custom_command(
    OUTPUT "${_arti_basic_lighting_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_basic_lighting_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_basic_lighting_shader_source}"
        "${_arti_basic_lighting_shader_output}"
    DEPENDS "${_arti_basic_lighting_shader_source}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_arti_basic_lighting_present_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_basic_lighting_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_basic_lighting_present_shader_source}"
        "${_arti_basic_lighting_present_shader_output}"
    DEPENDS "${_arti_basic_lighting_present_shader_source}"
    VERBATIM
)
add_custom_target(basic_lighting_assets
    DEPENDS
        "${_arti_basic_lighting_shader_output}"
        "${_arti_basic_lighting_present_shader_output}"
)
add_dependencies(basic_lighting basic_lighting_assets)

target_compile_definitions(basic_lighting PRIVATE
    ARTI_BASIC_LIGHTING_SHADER_PATH="${_arti_basic_lighting_shader_output}"
    ARTI_BASIC_LIGHTING_PRESENT_SHADER_PATH="${_arti_basic_lighting_present_shader_output}"
)
target_include_directories(basic_lighting PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(basic_lighting
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(basic_lighting)
set_target_properties(basic_lighting PROPERTIES FOLDER "Examples/Graphics")
