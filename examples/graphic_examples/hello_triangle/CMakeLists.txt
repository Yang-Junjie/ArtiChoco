add_executable(hello_triangle
    application.cpp
    hello_triangle_layer.cpp
    triangle_pass.cpp
)

set(_arti_hello_triangle_shader_source
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.slang"
)
set(_arti_hello_triangle_shader_directory
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
set(_arti_hello_triangle_shader_output
    "${_arti_hello_triangle_shader_directory}/triangle.slang"
)

add_custom_command(
    OUTPUT "${_arti_hello_triangle_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_hello_triangle_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_hello_triangle_shader_source}"
        "${_arti_hello_triangle_shader_output}"
    DEPENDS "${_arti_hello_triangle_shader_source}"
    VERBATIM
)
add_custom_target(hello_triangle_assets
    DEPENDS "${_arti_hello_triangle_shader_output}"
)
add_dependencies(hello_triangle hello_triangle_assets)

target_compile_definitions(hello_triangle PRIVATE
    ARTI_HELLO_TRIANGLE_SHADER_PATH="${_arti_hello_triangle_shader_output}"
)
target_include_directories(hello_triangle PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(hello_triangle
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
)

artichoco_stage_vulkan_sdk_runtime(hello_triangle)
set_target_properties(hello_triangle PROPERTIES FOLDER "Examples/Graphics")
