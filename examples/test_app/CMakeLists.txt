add_executable(test_app
    application.cpp
    camera_controller_system.cpp
    image_loader.cpp
    passes/mrt_composite_pass.cpp
    passes/mrt_mesh_pass.cpp
    passes/texture_compute_pass.cpp
    passes/throw_once_pass.cpp
    render_system.cpp
    test_app_layer.cpp
)

set(_arti_test_mesh_shader
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.slang"
)
set(_arti_test_shader_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(_arti_test_mesh_shader_output
    "${_arti_test_shader_directory}/mesh.slang"
)
set(_arti_test_compute_shader
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/texture_compute.slang"
)
set(_arti_test_compute_shader_output
    "${_arti_test_shader_directory}/texture_compute.slang"
)
set(_arti_test_composite_shader
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/mrt_composite.slang"
)
set(_arti_test_composite_shader_output
    "${_arti_test_shader_directory}/mrt_composite.slang"
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
add_custom_command(
    OUTPUT "${_arti_test_composite_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_test_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_test_composite_shader}"
        "${_arti_test_composite_shader_output}"
    DEPENDS "${_arti_test_composite_shader}"
    VERBATIM
)
add_custom_target(test_app_assets
    DEPENDS
        "${_arti_test_mesh_shader_output}"
        "${_arti_test_compute_shader_output}"
        "${_arti_test_composite_shader_output}"
        "${_arti_test_brownie_texture_output}"
)
add_dependencies(test_app test_app_assets)
target_compile_definitions(test_app PRIVATE
    ARTI_TEST_MESH_SHADER_PATH="${_arti_test_mesh_shader_output}"
    ARTI_TEST_COMPUTE_SHADER_PATH="${_arti_test_compute_shader_output}"
    ARTI_TEST_COMPOSITE_SHADER_PATH="${_arti_test_composite_shader_output}"
    ARTI_TEST_BROWNIE_TEXTURE_PATH="${_arti_test_brownie_texture_output}"
)
target_include_directories(test_app SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb"
)
target_include_directories(test_app PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/passes"
)

target_link_libraries(test_app
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Project
        ArtiChoco::Renderer
        ArtiChoco::Scene
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(test_app)
set_target_properties(test_app PROPERTIES FOLDER "Examples")

add_executable(scene_serialization_smoke scene_serialization_smoke.cpp)
target_link_libraries(scene_serialization_smoke PRIVATE ArtiChoco::Scene)
set_target_properties(scene_serialization_smoke PROPERTIES FOLDER "Examples")

add_executable(scene_cloner_smoke scene_cloner_smoke.cpp)
target_link_libraries(scene_cloner_smoke PRIVATE ArtiChoco::Scene)
set_target_properties(scene_cloner_smoke PROPERTIES FOLDER "Examples")

add_executable(scene_hierarchy_smoke scene_hierarchy_smoke.cpp)
target_link_libraries(scene_hierarchy_smoke PRIVATE ArtiChoco::Scene)
set_target_properties(scene_hierarchy_smoke PROPERTIES FOLDER "Examples")

add_executable(project_smoke project_smoke.cpp)
target_link_libraries(project_smoke PRIVATE ArtiChoco::Project)
set_target_properties(project_smoke PROPERTIES FOLDER "Examples")
