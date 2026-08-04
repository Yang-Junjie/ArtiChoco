add_executable(test_app
    application.cpp
    image_loader.cpp
    render_passes.cpp
    test_app_layer.cpp
)

set(_arti_test_mesh_shader
    "${PROJECT_SOURCE_DIR}/artichoco/renderer/shaders/mesh.slang"
)
set(_arti_test_shader_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(_arti_test_mesh_shader_output
    "${_arti_test_shader_directory}/mesh.slang"
)
set(_arti_test_compute_shader
    "${PROJECT_SOURCE_DIR}/artichoco/renderer/shaders/texture_compute.slang"
)
set(_arti_test_compute_shader_output
    "${_arti_test_shader_directory}/texture_compute.slang"
)
set(_arti_test_brownie_texture
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/brownie.png"
)
set(_arti_test_asset_directory "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_arti_test_brownie_texture_output
    "${_arti_test_asset_directory}/brownie.png"
)
add_custom_command(
    OUTPUT "${_arti_test_mesh_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_test_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_test_mesh_shader}"
        "${_arti_test_mesh_shader_output}"
    DEPENDS "${_arti_test_mesh_shader}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_arti_test_brownie_texture_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_test_asset_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_test_brownie_texture}"
        "${_arti_test_brownie_texture_output}"
    DEPENDS "${_arti_test_brownie_texture}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_arti_test_compute_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_test_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_test_compute_shader}"
        "${_arti_test_compute_shader_output}"
    DEPENDS "${_arti_test_compute_shader}"
    VERBATIM
)
add_custom_target(test_app_assets
    DEPENDS
        "${_arti_test_mesh_shader_output}"
        "${_arti_test_compute_shader_output}"
        "${_arti_test_brownie_texture_output}"
)
add_dependencies(test_app test_app_assets)
target_compile_definitions(test_app PRIVATE
    ARTI_TEST_MESH_SHADER_PATH="${_arti_test_mesh_shader_output}"
    ARTI_TEST_COMPUTE_SHADER_PATH="${_arti_test_compute_shader_output}"
    ARTI_TEST_BROWNIE_TEXTURE_PATH="${_arti_test_brownie_texture_output}"
)
target_include_directories(test_app SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb"
)

target_link_libraries(test_app
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(test_app)
set_target_properties(test_app PROPERTIES FOLDER "Examples")
