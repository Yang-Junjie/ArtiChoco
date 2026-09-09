include_guard(GLOBAL)

include(ArtiDependencyPolicy)

# SDL3：窗口 / 输入 / Vulkan surface 的来源。
#
# Windows 上是 SDK 里那对导入库 + DLL（Debug / Release 文件名不同，要分开配），
# Linux 上是发行版的共享库（只有一份，没有 d 后缀）。
#
# 不建 SHARED IMPORTED + INTERFACE_INCLUDE_DIRECTORIES 两套目标：一个 target 一份真相，
# 平台差异只体现在 IMPORTED_LOCATION 怎么填。

set(ARTI_SDL3_ROOT "" CACHE PATH "SDL3 installation prefix (default: the Vulkan SDK or the system)")

arti_dependency_hints(_arti_sdl3_hints ROOT "${ARTI_SDL3_ROOT}")
find_path(_arti_sdl3_include_dir
    NAMES SDL3/SDL.h
    HINTS ${_arti_sdl3_hints}
    PATH_SUFFIXES Include include
    NO_CACHE
)
arti_dependency_sibling_prefix(_arti_sdl3_prefix "${_arti_sdl3_include_dir}")
find_library(_arti_sdl3_library_release
    NAMES SDL3
    HINTS ${_arti_sdl3_hints} ${_arti_sdl3_prefix}
    PATH_SUFFIXES Lib lib
    NO_CACHE
)

if(NOT _arti_sdl3_include_dir OR NOT _arti_sdl3_library_release)
    message(FATAL_ERROR
        "SDL3 was not found (headers: '${_arti_sdl3_include_dir}', "
        "library: '${_arti_sdl3_library_release}').\n"
        "Install it (Arch: sdl3, Debian/Ubuntu: libsdl3-dev) or configure with "
        "-DARTI_SDL3_ROOT=<prefix>.")
endif()

if(WIN32)
    # 运行时 DLL 和导入库必须成对：Debug 用 SDL3d，Release 用 SDL3。两个都 REQUIRED ——
    # 少一个的话，症状是「Debug 能编、跑起来缺 DLL」，报错里不会提到是谁缺的。
    find_library(_arti_sdl3_library_debug
        NAMES SDL3d SDL3
        HINTS ${_arti_sdl3_hints} ${_arti_sdl3_prefix}
        PATH_SUFFIXES Lib lib
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_sdl3_runtime_release
        NAMES SDL3.dll
        HINTS ${_arti_sdl3_hints} ${_arti_sdl3_prefix}
        PATH_SUFFIXES Bin bin
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_sdl3_runtime_debug
        NAMES SDL3d.dll SDL3.dll
        HINTS ${_arti_sdl3_hints} ${_arti_sdl3_prefix}
        PATH_SUFFIXES Bin bin
        NO_CACHE
        REQUIRED
    )

    add_library(arti_sdk_sdl3 SHARED IMPORTED GLOBAL)
    set_target_properties(arti_sdk_sdl3 PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_IMPLIB "${_arti_sdl3_library_release}"
        IMPORTED_LOCATION "${_arti_sdl3_runtime_release}"
        IMPORTED_IMPLIB_DEBUG "${_arti_sdl3_library_debug}"
        IMPORTED_LOCATION_DEBUG "${_arti_sdl3_runtime_debug}"
        IMPORTED_IMPLIB_RELEASE "${_arti_sdl3_library_release}"
        IMPORTED_LOCATION_RELEASE "${_arti_sdl3_runtime_release}"
        MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
    )
else()
    # Linux / macOS：链接时按绝对路径给，运行时靠 SONAME —— 库不在默认搜索路径上时，
    # 由 artichoco_stage_libraries() 给消费方补 BUILD_RPATH（见 ArtiRuntimeStaging.cmake）。
    add_library(arti_sdk_sdl3 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(arti_sdk_sdl3 PROPERTIES
        IMPORTED_LOCATION "${_arti_sdl3_library_release}"
    )
endif()

add_library(ArtiSDK::SDL3 ALIAS arti_sdk_sdl3)
set_target_properties(arti_sdk_sdl3 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_arti_sdl3_include_dir}"
)
