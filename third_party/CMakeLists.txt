set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(spdlog EXCLUDE_FROM_ALL)
set_target_properties(spdlog PROPERTIES FOLDER "ThirdParty")

set(VMA_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(VMA EXCLUDE_FROM_ALL)
set_target_properties(VulkanMemoryAllocator PROPERTIES FOLDER "ThirdParty")
target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/VMA/include>"
)
target_compile_definitions(VulkanMemoryAllocator INTERFACE
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    VMA_STATIC_VULKAN_FUNCTIONS=1
)

add_library(artichoco_entt INTERFACE)
add_library(EnTT::EnTT ALIAS artichoco_entt)
target_include_directories(artichoco_entt SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/entt/src"
)
target_compile_features(artichoco_entt INTERFACE cxx_std_20)

set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(enkiTS EXCLUDE_FROM_ALL)
set_target_properties(enkiTS PROPERTIES FOLDER "ThirdParty")
