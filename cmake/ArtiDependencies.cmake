include_guard(GLOBAL)

# ArtiChoco 的第三方依赖入口。三处 `include(ArtiDependencies)`：ArtiEngine 根、ArtiRenderer、
# ArtiChoco 自己（include_guard(GLOBAL) 保证只执行一次，后两处是空操作）。
#
# **依赖分两类，规则只有一条：除了图形 API 和着色器编译器，全部自己编。**
# 完整清单、判据和每个依赖的版本 / 许可见 docs/Architecture/Dependencies.md。
#
#   external（交给机器 / SDK）—— 需要「发现」逻辑，所以各自一个模块：
#     ArtiVulkan.cmake    Vulkan 头 + loader（和驱动耦合，必须用机器上那份）
#     ArtiSlang.cmake     运行期着色器编译器（219MB 源码、30MB 库，没有发行版愿意编）
#   vendored（源码 submodule，我们自己编）—— 不需要发现，所以没有模块：
#     SDL3 / glm / spdlog / yaml-cpp / entt / enkiTS / stb / nvrhi 在
#     ArtiChoco/third_party/CMakeLists.txt；imgui 在 ArtiRenderer/third_party/；
#     box3d / lua / sol2 / ImGuizmo / cgltf / nativefiledialog-extended 在根 third_party/。
#
# 两类的共同点：**对外都只暴露 ArtiSDK::* 契约目标**，消费方不关心它从哪来。
# （`ArtiSDK::` 这个名字是历史包袱 —— 现在它一半是 vendored，改名是下一轮清理。）
#
# 这一层负责的两件事：**external 的发现** + **staging**（让消费方在构建树 / 产物里能加载库）。

include(ArtiVulkan)
include(ArtiSlang)
include(ArtiRuntimeStaging)

message(STATUS "ArtiChoco dependencies:")
message(STATUS "  Vulkan: ${Vulkan_VERSION}  ${_arti_vulkan_library}")
message(STATUS "  Slang:  ${_arti_slang_library_release}")
message(STATUS "  slangc: ${_arti_slangc_executable}")
message(STATUS "  vendored: SDL3 / glm / spdlog / yaml-cpp / entt / enkiTS / nvrhi / imgui / …")
