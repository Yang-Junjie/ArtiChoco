include_guard(GLOBAL)

# 第三方依赖的统一查找策略。
#
# 每个依赖都按**同一个顺序**找：显式缓存变量 → Vulkan SDK 前缀 → CMake 默认搜索路径。
# 用 HINTS 而不是 NO_DEFAULT_PATH 来表达「优先但不排斥」：HINTS 排在默认路径之前，所以
# Windows 上仍然先命中 SDK 里那一份（解析结果与改动前逐字一致），而 Linux 上 SDK 里没有的
# 依赖（SDL3 / glm）自然回落到发行版的包。
#
# 为什么不是「要么全在 SDK 里、要么全走系统」：Windows 的 LunarG SDK 四样都带，Linux 的
# LunarG SDK 只带 vulkan + slang —— 一刀切会让 Linux 永远走不进去。

set(ARTI_VULKAN_SDK "" CACHE PATH "Vulkan SDK installation prefix")
if(NOT ARTI_VULKAN_SDK AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" _arti_vulkan_sdk_from_environment)
    set(ARTI_VULKAN_SDK "${_arti_vulkan_sdk_from_environment}" CACHE PATH
        "Vulkan SDK installation prefix" FORCE)
endif()

# SDK 的候选前缀。两种布局都见过：
#   Windows / macOS：<sdk>/{Include,Lib,Bin}
#   Linux：<sdk>/x86_64/{include,lib,bin} —— 而且 setup-env.sh 把 VULKAN_SDK 直接指到那个
#   x86_64 目录，所以根目录和 x86_64 两种都要留着。
set(ARTI_SDK_PREFIXES)
if(ARTI_VULKAN_SDK)
    file(REAL_PATH "${ARTI_VULKAN_SDK}" ARTI_VULKAN_SDK_ROOT)
    if(NOT IS_DIRECTORY "${ARTI_VULKAN_SDK_ROOT}")
        message(FATAL_ERROR "ARTI_VULKAN_SDK does not exist: ${ARTI_VULKAN_SDK}")
    endif()
    list(APPEND ARTI_SDK_PREFIXES
        "${ARTI_VULKAN_SDK_ROOT}"
        "${ARTI_VULKAN_SDK_ROOT}/x86_64"
        "${ARTI_VULKAN_SDK_ROOT}/aarch64"
        "${ARTI_VULKAN_SDK_ROOT}/arm64"
        "${ARTI_VULKAN_SDK_ROOT}/macOS"
    )
endif()

# arti_dependency_hints(<out-var> [ROOT <prefix>])
#
# 某个依赖的 HINTS 列表：显式前缀在前，SDK 前缀在后。
function(arti_dependency_hints out_var)
    cmake_parse_arguments(ARG "" "ROOT" "" ${ARGN})
    set(_hints)
    if(ARG_ROOT)
        list(APPEND _hints "${ARG_ROOT}")
    endif()
    list(APPEND _hints ${ARTI_SDK_PREFIXES})
    set(${out_var} "${_hints}" PARENT_SCOPE)
endfunction()

# arti_dependency_sibling_prefix(<out-var> <include-dir>)
#
# 头文件所在目录的上一级 —— 库通常就在同一个前缀的 lib/ 下（`<prefix>/include/x` 与
# `<prefix>/lib/libx.so`）。把它也放进 HINTS，能让「头在这、库也在这」的自定义前缀
# （ARTI_*_ROOT 指过去的那种）自动成对命中，不用再传一次库的前缀。
function(arti_dependency_sibling_prefix out_var include_dir)
    set(_prefix "")
    if(include_dir)
        get_filename_component(_prefix "${include_dir}" DIRECTORY)
    endif()
    set(${out_var} "${_prefix}" PARENT_SCOPE)
endfunction()
