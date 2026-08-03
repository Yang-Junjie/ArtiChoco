add_executable(test_app
    application.cpp
    test_app_layer.cpp
)

target_link_libraries(test_app
    PRIVATE
        ArtiChoco::Core
        ArtiChoco::Platform
)

artichoco_stage_vulkan_sdk_runtime(test_app)
set_target_properties(test_app PROPERTIES FOLDER "Examples")
