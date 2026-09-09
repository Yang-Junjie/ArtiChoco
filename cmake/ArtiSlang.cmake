include_guard(GLOBAL)

include(ArtiDependencyPolicy)

# Slang：运行期着色器编译器（libslang + slangc）。
#
# 两个坑，都在这里挡掉：
#
# 1. **同名不同物。** Arch 的 `slang` 包是 S-Lang（一个终端脚本语言），它的头也在
#    `/usr/include/slang/slang.h` —— 按 `slang/slang.h` 去找会命中它。所以这里认的标记文件是
#    `slang-com-ptr.h`：只有着色器 Slang 有，而且它和 slang.h 永远在同一个目录。
# 2. **布局不止一种。** LunarG SDK 是 `include/slang/slang.h`，官方 release tarball 和 AUR 的
#    shader-slang-bin 是扁平的 `include/slang.h`。所以这里暴露的是**含 slang.h 的那个目录**，
#    源码统一写 `<slang.h>`（见 artichoco/renderer/slang_compiler.cpp），三种布局都能用。

set(ARTI_SLANG_ROOT "" CACHE PATH "Slang installation prefix (default: the Vulkan SDK or the system)")

arti_dependency_hints(_arti_slang_hints ROOT "${ARTI_SLANG_ROOT}")
list(APPEND _arti_slang_hints /opt/shader-slang-bin)

find_path(_arti_slang_include_dir
    NAMES slang-com-ptr.h
    HINTS ${_arti_slang_hints}
    PATH_SUFFIXES Include include Include/slang include/slang
                  Include/shader-slang include/shader-slang shader-slang
    NO_CACHE
)
if(_arti_slang_include_dir AND NOT EXISTS "${_arti_slang_include_dir}/slang.h")
    set(_arti_slang_include_dir "_arti_slang_include_dir-NOTFOUND")
endif()
arti_dependency_sibling_prefix(_arti_slang_prefix "${_arti_slang_include_dir}")

find_library(_arti_slang_library_release
    NAMES slang
    HINTS ${_arti_slang_hints} ${_arti_slang_prefix}
    PATH_SUFFIXES Lib lib Lib/shader-slang lib/shader-slang
    NO_CACHE
)
find_program(_arti_slangc_executable
    NAMES slangc slangc.exe
    HINTS ${_arti_slang_hints} ${_arti_slang_prefix}
    PATH_SUFFIXES Bin bin
    NO_CACHE
)

if(NOT _arti_slang_include_dir OR NOT _arti_slang_library_release)
    message(FATAL_ERROR
        "Slang was not found (headers: '${_arti_slang_include_dir}', "
        "library: '${_arti_slang_library_release}').\n"
        "It ships with the Vulkan SDK (set VULKAN_SDK / -DARTI_VULKAN_SDK=<prefix>), or install "
        "it separately (Arch: shader-slang-bin) and configure with -DARTI_SLANG_ROOT=<prefix>.\n"
        "Note: the package literally named 'slang' on Arch is S-Lang, not this compiler.")
endif()

if(WIN32)
    # Slang 上游只发一份 DLL，没有 debug / release 之分。SDK 的 Lib 下确实有 slangd.lib，
    # 但那是 language server（slangd.exe 的伴生件）的导入库，**内嵌的 DLL 名同样是 slang.dll**。
    # 以前这里 debug 配置链 slangd.lib、却把 slangd.dll 当运行时依赖，两边对不上：staging
    # 拷过去的文件 exe 根本不加载，真正需要的 slang.dll 一个都不拷。所以两个配置统一。
    find_file(_arti_slang_runtime_release
        NAMES slang.dll
        HINTS ${_arti_slang_hints} ${_arti_slang_prefix}
        PATH_SUFFIXES Bin bin
        NO_CACHE
        REQUIRED
    )

    # slang.dll 只有几十 KB，是个转发器 —— 实现在 slang-compiler.dll（20 MB 量级）里，由它在
    # 运行时 LoadLibrary 加载。CMake 的 $<TARGET_RUNTIME_DLLS> 只跟 target 依赖图，看不见
    # 动态加载，所以 artichoco_stage_libraries() 里必须显式拷这一个。
    #
    # 只有它。slang-glslang / slang-rt / slang-glsl-module 实测都不需要（走 SPIR-V 直出，
    # 不过 glslang），三个加起来 30 MB+，别顺手都拷进去。
    find_file(_arti_slang_compiler_runtime
        NAMES slang-compiler.dll
        HINTS ${_arti_slang_hints} ${_arti_slang_prefix}
        PATH_SUFFIXES Bin bin
        NO_CACHE
        REQUIRED
    )
    # CACHE INTERNAL：staging 函数在**调用点**展开，而调用点在别的目录作用域里。
    set(ARTICHOCO_SLANG_COMPILER_RUNTIME "${_arti_slang_compiler_runtime}" CACHE INTERNAL
        "slang-compiler.dll, loaded dynamically by slang.dll")

    add_library(arti_sdk_slang SHARED IMPORTED GLOBAL)
    set_target_properties(arti_sdk_slang PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_IMPLIB "${_arti_slang_library_release}"
        IMPORTED_LOCATION "${_arti_slang_runtime_release}"
        IMPORTED_IMPLIB_DEBUG "${_arti_slang_library_release}"
        IMPORTED_LOCATION_DEBUG "${_arti_slang_runtime_release}"
        IMPORTED_IMPLIB_RELEASE "${_arti_slang_library_release}"
        IMPORTED_LOCATION_RELEASE "${_arti_slang_runtime_release}"
        MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
    )
else()
    add_library(arti_sdk_slang UNKNOWN IMPORTED GLOBAL)
    set_target_properties(arti_sdk_slang PROPERTIES
        IMPORTED_LOCATION "${_arti_slang_library_release}"
    )
endif()

add_library(ArtiSDK::Slang ALIAS arti_sdk_slang)
set_target_properties(arti_sdk_slang PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_arti_slang_include_dir}"
)

# slangc：目前没有构建步骤消费它（着色器是运行期由 libslang 编译的），保留是因为
# 「SDK 面」应该完整 —— 将来做离线预编译时直接用，不必再改发现逻辑。
if(_arti_slangc_executable)
    add_executable(arti_sdk_slangc IMPORTED GLOBAL)
    add_executable(ArtiSDK::SlangC ALIAS arti_sdk_slangc)
    set_target_properties(arti_sdk_slangc PROPERTIES
        IMPORTED_LOCATION "${_arti_slangc_executable}"
    )
endif()
