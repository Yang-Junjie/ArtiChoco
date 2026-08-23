add_executable(instancing
    application.cpp
    instancing_layer.cpp
    instancing_pass.cpp
)

set(_arti_instancing_shader_directory
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
set(_arti_instancing_shader_source
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/instancing.slang"
)
set(_arti_instancing_shader_output
    "${_arti_instancing_shader_directory}/instancing.slang"
)

add_custom_command(
    OUTPUT "${_arti_instancing_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_instancing_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_instancing_shader_source}"
        "${_arti_instancing_shader_output}"
    DEPENDS "${_arti_instancing_shader_source}"
    VERBATIM
)
add_custom_target(instancing_assets
    DEPENDS "${_arti_instancing_shader_output}"
)
add_dependencies(instancing instancing_assets)

target_compile_definitions(instancing PRIVATE
    ARTI_INSTANCING_SHADER_PATH="${_arti_instancing_shader_output}"
)
target_include_directories(instancing PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(instancing
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(instancing)
set_target_properties(instancing PROPERTIES FOLDER "Examples/Graphics")
