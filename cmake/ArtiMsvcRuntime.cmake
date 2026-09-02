include_guard(GLOBAL)

# MSVC CRT 的可再分发 DLL。
#
# 为什么需要它：产物用动态 CRT（/MD），所以运行时要 msvcp140.dll / vcruntime140.dll 那几个。
# 它们**不是 Windows 自带的** —— 靠 VC++ Redistributable，而 api-ms-win-crt-*（UCRT）才是系统
# 自带的。$<TARGET_RUNTIME_DLLS> 也拿不到它们（CRT 不是 CMake target）。不带上就是「在装过
# VS 的机器上能跑，别人那儿缺 DLL」，而那台机器的报错里不会提到是谁缺的。
#
# 为什么不用 CMake 自己的 InstallRequiredSystemLibraries：那个模块是 `if(MSVC)` 把门的，而本
# 项目用独立 clang 走 MSVC ABI，CMake 此时 MSVC 和 MSVC_TOOLSET_VERSION 都是空的
# （CMAKE_CXX_COMPILER_FRONTEND_VARIANT 是 GNU），模块会静默什么都不做。实测过。
#
# 为什么从 VS 的 Redist 目录取而不是从 System32 拷：那里的副本就是给分发用的，而且 clang 要
# targeting MSVC ABI 本来就需要 MSVC 的库和头，所以 VS / Build Tools 已经是本项目的硬前提，
# 依赖它不引入新前提。

set(ARTI_MSVC_REDIST_DIR "" CACHE PATH
    "Microsoft.VC<n>.CRT redistributable directory (auto-detected from Visual Studio)")

if(NOT WIN32)
    # 非 Windows 上 CRT 这个概念不存在，函数留成空操作，调用方不必加 if。
    function(artichoco_stage_msvc_runtime target)
    endfunction()
    return()
endif()

# 1. 显式指定优先。2. 开发者命令提示符里的 VCToolsRedistDir。3. vswhere 找 VS 安装位置。
if(NOT ARTI_MSVC_REDIST_DIR AND NOT "$ENV{VCToolsRedistDir}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" _arti_vc_redist_root)
    file(GLOB _arti_crt_candidates "${_arti_vc_redist_root}/x64/Microsoft.VC*.CRT")
    if(_arti_crt_candidates)
        list(SORT _arti_crt_candidates COMPARE NATURAL)
        list(GET _arti_crt_candidates -1 ARTI_MSVC_REDIST_DIR)
    endif()
endif()

if(NOT ARTI_MSVC_REDIST_DIR)
    find_program(_arti_vswhere vswhere
        HINTS
            "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer"
            "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer"
        NO_CACHE
    )
    if(_arti_vswhere)
        execute_process(
            COMMAND "${_arti_vswhere}" -latest -products * -property installationPath
            OUTPUT_VARIABLE _arti_vs_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_arti_vs_path)
            file(TO_CMAKE_PATH "${_arti_vs_path}" _arti_vs_path)
            # 布局是 <VS>/VC/Redist/MSVC/<version>/x64/Microsoft.VC<toolset>.CRT。
            # 同一个 VS 下会并存好几个 <version>，取自然序最大的那个。
            file(GLOB _arti_crt_candidates
                "${_arti_vs_path}/VC/Redist/MSVC/*/x64/Microsoft.VC*.CRT")
            if(_arti_crt_candidates)
                list(SORT _arti_crt_candidates COMPARE NATURAL)
                list(GET _arti_crt_candidates -1 ARTI_MSVC_REDIST_DIR)
            endif()
        endif()
    endif()
endif()

if(ARTI_MSVC_REDIST_DIR)
    # 整个目录都拷，不只挑当前导入表里那三个（msvcp140 / msvcp140_atomic_wait / vcruntime140）。
    # 多出来的七个加起来约 1.1 MB，而产物本身是百 MB 量级；换来的是「以后哪个 target 用到
    # concrt / msvcp140_2 之类时不会静默缺文件」—— 那种缺失只在别人的机器上才暴露。
    file(GLOB _arti_msvc_redist_dlls "${ARTI_MSVC_REDIST_DIR}/*.dll")
endif()

if(_arti_msvc_redist_dlls)
    set(ARTICHOCO_MSVC_REDIST_DLLS "${_arti_msvc_redist_dlls}" CACHE INTERNAL
        "MSVC CRT redistributable DLLs staged next to executables")
    list(LENGTH _arti_msvc_redist_dlls _arti_msvc_redist_count)
    message(STATUS "ArtiChoco MSVC CRT redist: ${ARTI_MSVC_REDIST_DIR} "
                   "(${_arti_msvc_redist_count} dll)")
else()
    set(ARTICHOCO_MSVC_REDIST_DLLS "" CACHE INTERNAL
        "MSVC CRT redistributable DLLs staged next to executables")
    # 警告而不是 FATAL_ERROR：找不到 CRT 只影响产物能不能拿去别的机器，不影响本机构建和运行
    # （能构建说明本机装着 CRT）。为一个打包问题让整个构建配不起来不划算。
    message(WARNING
        "MSVC CRT redistributable directory was not found. Builds still run on this machine, "
        "but packaged output will depend on the VC++ Redistributable being installed. "
        "Set -DARTI_MSVC_REDIST_DIR=<path-to-Microsoft.VCxxx.CRT> to fix.")
endif()

# 把 CRT 的可再分发 DLL 拷到 target 旁边。和 artichoco_stage_vulkan_sdk_runtime() 一个形状：
# 由消费方 exe 调用。找不到 CRT 时是空操作（上面已经警告过了）。
function(artichoco_stage_msvc_runtime target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot stage the MSVC runtime for unknown target: ${target}")
    endif()
    if(NOT ARTICHOCO_MSVC_REDIST_DLLS)
        return()
    endif()
    # Debug 构建跳过：它链的是调试 CRT（msvcp140d / vcruntime140d / ucrtbased），而 redist 目录
    # 里只有 release 那一套 —— 拷过去一个都不会被加载，只是在输出目录里堆十个没人用的文件，
    # 让人以为 Debug 产物也是可发布的。调试 CRT 本身不可再分发，所以发布只能用 Release。
    #
    # 单配置生成器（本项目用 Ninja）在配置期就知道 CMAKE_BUILD_TYPE。多配置生成器下它是空的，
    # 这个判断为假、照常拷 —— 那种情况下宁可多拷也不要少拷。
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        return()
    endif()

    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            ${ARTICHOCO_MSVC_REDIST_DLLS}
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()
