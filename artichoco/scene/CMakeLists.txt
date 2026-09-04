add_library(artichoco_scene STATIC
    scene.cpp
    scene_cloner.cpp
    scene_entity_storage.cpp
    scene_hierarchy.cpp
    scene_log.cpp
    scene_serialization_registry.cpp
    scene_serializer.cpp
    scene_system_manager.cpp
)
add_library(ArtiChoco::Scene ALIAS artichoco_scene)
artichoco_configure_library(artichoco_scene)
target_link_libraries(artichoco_scene
    PUBLIC
        ArtiChoco::Core
        ArtiSDK::GLM
        EnTT::EnTT
        yaml-cpp::yaml-cpp
)

# 和 core 的 task_system_test 一样：纯 CPU、不开窗、不碰 GPU、不读资产，所以不跟着上游那条
# 「被当依赖库消费时把 tests 关掉」的先例走。复制实体的错法几乎都是**不报错**的
# （父级接错、名字重了、UUID 没登记），肉眼过代码抓不住，跟着每次 ctest 跑才有意义。
if(BUILD_TESTING)
    add_executable(scene_duplicate_test tests/scene_duplicate_test.cpp)
    target_compile_features(scene_duplicate_test PRIVATE cxx_std_20)
    target_link_libraries(scene_duplicate_test PRIVATE artichoco_scene)
    set_target_properties(scene_duplicate_test PROPERTIES FOLDER "Tests/ArtiChoco")
    add_test(NAME scene_duplicate_test COMMAND scene_duplicate_test)
    if(COMMAND artichoco_stage_msvc_runtime)
        artichoco_stage_msvc_runtime(scene_duplicate_test)
    endif()
endif()
