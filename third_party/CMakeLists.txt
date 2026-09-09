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

# SDL3：窗口 / 输入 / Vulkan surface。**vendored 而不是系统包**（见
# docs/Architecture/Dependencies.md 的判据）：
#   - 它是引擎的窗口系统，版本漂移会改变输入 / IME / 窗口行为，而发行版各有各的版本；
#   - 静态链进产物之后，发布出去的播放器少一个运行时依赖（Linux 不再要 libSDL3.so，
#     Windows 不再要 SDL3.dll）；
#   - 它不碰驱动，只碰 OS 的窗口 API，所以「自己编」不会引入和驱动不一致的风险。
#
# SDL_SHARED=OFF + SDL_STATIC=ON：只编静态库。它自己的 CMakeLists 会把 `SDL3::SDL3`
# 别名指到 static（见上游第 504～506 行），所以契约目标跟着它走就行。
# 头文件在 Windows 上的 declspec 只在 DLL_EXPORT（共享构建的私有定义）下才有值，
# 静态消费方不需要额外定义。
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL3 EXCLUDE_FROM_ALL)
set_target_properties(SDL3-static PROPERTIES FOLDER "ThirdParty")

# 契约目标。**不用 `add_library(ArtiSDK::SDL3 ALIAS SDL3::SDL3)`** —— CMake 不允许
# 给 alias 再起 alias（实测报 "cannot create ALIAS target ... is itself an ALIAS"），
# 而且夹一层 INTERFACE 之后，将来要在契约上加编译定义 / 修正 usage requirement 不用动消费方。
add_library(arti_sdk_sdl3 INTERFACE)
add_library(ArtiSDK::SDL3 ALIAS arti_sdk_sdl3)
target_link_libraries(arti_sdk_sdl3 INTERFACE SDL3::SDL3)

# glm：纯头数学库。vendored 的理由是「零编译代价 + 版本固定」—— 它不碰 OS、不碰驱动，
# 放给机器只会让「同一份代码在不同发行版上算出不同的默认行为」这件事变得可能。
# GLM_BUILD_LIBRARY=OFF 要的是 INTERFACE target（glm::glm），不是它可选的那个编译产物。
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glm EXCLUDE_FROM_ALL)

add_library(arti_sdk_glm INTERFACE)
add_library(ArtiSDK::GLM ALIAS arti_sdk_glm)
target_link_libraries(arti_sdk_glm INTERFACE glm::glm)
