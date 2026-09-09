include_guard(GLOBAL)

include(ArtiDependencyPolicy)

# Vulkan：头文件 + loader。
#
# 找到之后要喂给 CMake 自己的 FindVulkan：nvrhi 需要 Vulkan::Headers 这个 target
# （它自己的 CMake 只在 TARGET Vulkan::Headers 存在时才用系统头，否则会去 FetchContent
# 拉一份 Vulkan-Headers —— 本项目刻意不联网取依赖，所以这条路必须通）。
# FindVulkan 在 Vulkan_INCLUDE_DIR / Vulkan_LIBRARY 已经设好的情况下会直接用它们。

arti_dependency_hints(_arti_vulkan_hints)
find_path(_arti_vulkan_include_dir
    NAMES vulkan/vulkan.h
    HINTS ${_arti_vulkan_hints}
    PATH_SUFFIXES Include include
    NO_CACHE
)
find_library(_arti_vulkan_library
    NAMES vulkan-1 vulkan
    HINTS ${_arti_vulkan_hints}
    PATH_SUFFIXES Lib lib
    NO_CACHE
)

if(NOT _arti_vulkan_include_dir OR NOT _arti_vulkan_library)
    message(FATAL_ERROR
        "Vulkan was not found (headers: '${_arti_vulkan_include_dir}', "
        "loader: '${_arti_vulkan_library}').\n"
        "Install the Vulkan SDK and set VULKAN_SDK / -DARTI_VULKAN_SDK=<prefix>, or install "
        "the distribution packages (Arch: vulkan-headers + vulkan-icd-loader, "
        "Debian/Ubuntu: libvulkan-dev).")
endif()

set(Vulkan_INCLUDE_DIR "${_arti_vulkan_include_dir}" CACHE PATH "Vulkan include directory" FORCE)
set(Vulkan_LIBRARY "${_arti_vulkan_library}" CACHE FILEPATH "Vulkan loader library" FORCE)
find_package(Vulkan 1.4 REQUIRED)

add_library(arti_sdk_vulkan INTERFACE)
add_library(ArtiSDK::Vulkan ALIAS arti_sdk_vulkan)
target_link_libraries(arti_sdk_vulkan INTERFACE Vulkan::Vulkan)
if(APPLE)
    target_compile_definitions(arti_sdk_vulkan INTERFACE VK_ENABLE_BETA_EXTENSIONS)
endif()
