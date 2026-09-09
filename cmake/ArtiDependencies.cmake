include_guard(GLOBAL)

# ArtiChoco 的第三方依赖入口。三处 `include(ArtiDependencies)`：ArtiEngine 根、ArtiRenderer、
# ArtiChoco 自己（include_guard(GLOBAL) 保证只执行一次，后两处是空操作）。
#
# 这一层只做两件事，各自一个模块：
#   1. **发现**：ArtiVulkan / ArtiGLM / ArtiSDL3 / ArtiSlang，共用一条查找策略
#      （ArtiDependencyPolicy：显式变量 → SDK 前缀 → 系统）。
#   2. **staging**：ArtiRuntimeStaging，让消费方在构建树 / 产物里能加载这些库。
#
# 对外只暴露 ArtiSDK::Vulkan / GLM / SDL3 / Slang / SlangC 五个目标，和
# artichoco_stage_libraries() 一个函数 —— 下游不需要知道依赖是怎么找到的。

include(ArtiVulkan)
include(ArtiGLM)
include(ArtiSDL3)
include(ArtiSlang)
include(ArtiRuntimeStaging)

message(STATUS "ArtiChoco dependencies:")
message(STATUS "  Vulkan: ${Vulkan_VERSION}  ${_arti_vulkan_library}")
message(STATUS "  GLM:    ${_arti_glm_include_dir}")
message(STATUS "  SDL3:   ${_arti_sdl3_library_release}")
message(STATUS "  Slang:  ${_arti_slang_library_release}")
message(STATUS "  slangc: ${_arti_slangc_executable}")
