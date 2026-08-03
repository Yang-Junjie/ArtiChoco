add_executable(test_app
    application.cpp
    test_app_layer.cpp
)

set(_arti_test_triangle_shader
    "${PROJECT_SOURCE_DIR}/artichoco/renderer/shaders/triangle.slang"
)
set(_arti_test_shader_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(_arti_test_triangle_shader_output
    "${_arti_test_shader_directory}/triangle.slang"
)
add_custom_command(
    OUTPUT "${_arti_test_triangle_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_test_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_test_triangle_shader}"
        "${_arti_test_triangle_shader_output}"
    DEPENDS "${_arti_test_triangle_shader}"
    VERBATIM
)
add_custom_target(test_app_shaders
    DEPENDS "${_arti_test_triangle_shader_output}"
)
add_dependencies(test_app test_app_shaders)
target_compile_definitions(test_app PRIVATE
    ARTI_TEST_TRIANGLE_SHADER_PATH="${_arti_test_triangle_shader_output}"
)

target_link_libraries(test_app
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
)

artichoco_stage_vulkan_sdk_runtime(test_app)
set_target_properties(test_app PROPERTIES FOLDER "Examples")
