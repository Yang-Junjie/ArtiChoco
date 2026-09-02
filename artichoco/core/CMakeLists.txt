add_library(artichoco_core STATIC
    application.cpp
    entry_point.cpp
    io/input.cpp
    io/paths.cpp
    layer_stack.cpp
    log.cpp
    task/task_system.cpp
    task/thread_naming.cpp
    uuid.cpp
    window.cpp
)
add_library(ArtiChoco::Core ALIAS artichoco_core)

artichoco_configure_library(artichoco_core)
target_include_directories(artichoco_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/event"
        "${CMAKE_CURRENT_SOURCE_DIR}/io"
)
target_link_libraries(artichoco_core
    PUBLIC
        ArtiSDK::GLM
        spdlog::spdlog
        enkiTS
)

# 任务系统是这一层唯一有测试的东西，而且它刻意**不跟着**上游那条「被当依赖库消费时把 tests
# 关掉」的先例走（ArtiEngine 的根 CMakeLists 就那样关掉了 ArtiRenderer 的 tests）。理由：
# 这个测试纯 CPU、不开窗、不碰 GPU、不读资产，代价接近于零；而任务系统现在没有真实消费者，
# 让它每次 ctest 都跑是「没人练它」唯一的对冲。
if(BUILD_TESTING)
    add_executable(task_system_test tests/task_system_test.cpp)
    target_compile_features(task_system_test PRIVATE cxx_std_20)
    target_link_libraries(task_system_test PRIVATE artichoco_core)
    set_target_properties(task_system_test PROPERTIES FOLDER "Tests/ArtiChoco")
    add_test(NAME task_system_test COMMAND task_system_test)
    # 一个 DLL 都不链，所以不用 stage Vulkan SDK。CRT 那套还是要的 —— 但 ArtiMsvcRuntime
    # 是上游（ArtiEngine）include 进来的，ArtiChoco 单独构建时没有它。
    if(COMMAND artichoco_stage_msvc_runtime)
        artichoco_stage_msvc_runtime(task_system_test)
    endif()
endif()
