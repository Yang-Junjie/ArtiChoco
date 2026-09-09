include_guard(GLOBAL)

include(ArtiDependencyPolicy)

# GLM：纯头。没有库要链，所以只需要一个 include 目录。
#
# 不用 find_package(glm)：SDK 里那份没有 CMake config，系统包有没有也不一定 ——
# 直接找头文件反而覆盖两种来源，而且和 ArtiSDK::GLM 这个「只给 include 目录」的目标语义一致。

set(ARTI_GLM_ROOT "" CACHE PATH "GLM installation prefix (default: the Vulkan SDK or the system)")

arti_dependency_hints(_arti_glm_hints ROOT "${ARTI_GLM_ROOT}")
find_path(_arti_glm_include_dir
    NAMES glm/glm.hpp
    HINTS ${_arti_glm_hints}
    PATH_SUFFIXES Include include
    NO_CACHE
)

if(NOT _arti_glm_include_dir)
    message(FATAL_ERROR
        "GLM was not found.\n"
        "Install it (Arch: glm, Debian/Ubuntu: libglm-dev) or configure with "
        "-DARTI_GLM_ROOT=<prefix>.")
endif()

add_library(arti_sdk_glm INTERFACE)
add_library(ArtiSDK::GLM ALIAS arti_sdk_glm)
target_include_directories(arti_sdk_glm SYSTEM INTERFACE "${_arti_glm_include_dir}")
