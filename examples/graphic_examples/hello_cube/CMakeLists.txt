add_executable(hello_cube
    application.cpp
    cube_pass.cpp
    hello_cube_layer.cpp
    image_loader.cpp
)

set(_arti_hello_cube_shader_source
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/cube.slang"
)
set(_arti_hello_cube_shader_directory
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
set(_arti_hello_cube_shader_output
    "${_arti_hello_cube_shader_directory}/cube.slang"
)
set(_arti_hello_cube_image_source
    "${CMAKE_CURRENT_SOURCE_DIR}/asset/image.png"
)
set(_arti_hello_cube_asset_directory
    "${CMAKE_CURRENT_BINARY_DIR}/asset"
)
set(_arti_hello_cube_image_output
    "${_arti_hello_cube_asset_directory}/image.png"
)

add_custom_command(
    OUTPUT "${_arti_hello_cube_shader_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_hello_cube_shader_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_hello_cube_shader_source}"
        "${_arti_hello_cube_shader_output}"
    DEPENDS "${_arti_hello_cube_shader_source}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${_arti_hello_cube_image_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arti_hello_cube_asset_directory}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_arti_hello_cube_image_source}"
        "${_arti_hello_cube_image_output}"
    DEPENDS "${_arti_hello_cube_image_source}"
    VERBATIM
)
add_custom_target(hello_cube_assets
    DEPENDS
        "${_arti_hello_cube_shader_output}"
        "${_arti_hello_cube_image_output}"
)
add_dependencies(hello_cube hello_cube_assets)

target_compile_definitions(hello_cube PRIVATE
    ARTI_HELLO_CUBE_SHADER_PATH="${_arti_hello_cube_shader_output}"
    ARTI_HELLO_CUBE_IMAGE_PATH="${_arti_hello_cube_image_output}"
)
target_include_directories(hello_cube SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb"
)
target_include_directories(hello_cube PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(hello_cube
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
        ArtiChoco::Renderer
        ArtiSDK::GLM
)

artichoco_stage_vulkan_sdk_runtime(hello_cube)
set_target_properties(hello_cube PROPERTIES FOLDER "Examples/Graphics")
