set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(spdlog EXCLUDE_FROM_ALL)
set_target_properties(spdlog PROPERTIES FOLDER "ThirdParty")

set(NVRHI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL_EXPORTS OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(NVRHI_FETCH_VULKAN_HEADERS OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12 OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_NVAPI OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_AFTERMATH OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_RTXMU OFF CACHE BOOL "" FORCE)
add_subdirectory(nvrhi EXCLUDE_FROM_ALL)

# **补上 nvrhi 漏掉的一条依赖。** nvrhi 的 CMakeLists 里 `nvrhi_vk` 只链了 Vulkan 头，
# 但它的 .o 引用 `nvrhi` 里的符号（verifyPermanentResourceState、AftermathCrashDumpHelper…
# 都在 src/common/*.cpp，属于 `nvrhi`）。MSVC 的链接器不在乎静态库的先后顺序，所以上游没暴露；
# GNU ld 是按命令行顺序一遍扫过的，`nvrhi` 排在 `nvrhi_vk` 前面就会得到一堆 undefined reference。
#
# 在消费方补，而不是改 submodule 里的 nvrhi：改上游会在每次升级时冲突，而这个事实是
# 「我们怎么用 nvrhi」的一部分。INTERFACE 方向才是对的 —— 谁链了 nvrhi_vk，谁就在它后面
# 拿到 nvrhi。
if(TARGET nvrhi_vk AND TARGET nvrhi)
    target_link_libraries(nvrhi_vk INTERFACE nvrhi)
endif()

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

set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_USE_STRICT_FLAGS OFF CACHE BOOL "" FORCE)
add_subdirectory(yaml-cpp EXCLUDE_FROM_ALL)
set_target_properties(yaml-cpp PROPERTIES FOLDER "ThirdParty")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(yaml-cpp PUBLIC -Wno-ignored-attributes)
endif()
