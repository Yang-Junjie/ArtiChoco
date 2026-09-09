include_guard(GLOBAL)

include(ArtiMsvcRuntime)

# 让一个 target 在**构建树里和打包产物里**都能加载它链的库。
#
# 这是「发现依赖」之后的另一件事，所以单独一层：发现回答「头文件和库在哪」，staging 回答
# 「运行时去哪找它们」。两个平台的机制不同，但**调用方只该知道一件事**：
#
#   artichoco_stage_libraries(<target>)
#
#   Windows：把导入表里的 DLL（$<TARGET_RUNTIME_DLLS>）+ 动态加载的 slang-compiler.dll
#            + CRT 可再分发 DLL 拷到 exe 旁边。
#   Linux：  给 target 补 BUILD_RPATH（指向不在默认搜索路径上的依赖目录），不拷库 ——
#            在 Linux 上把 .so 拷到 exe 旁边是反模式，发行版库应该由系统提供。
#
# 以前这里是两个函数（artichoco_stage_vulkan_sdk_runtime + artichoco_stage_msvc_runtime），
# 每个 exe 都得记得按顺序调两遍；而名字里的「vulkan_sdk」也不对（它拷的是 SDL3 / slang / CRT）。
# 合成一个之后，「要发布这个 exe 就得调它」这件事只剩一个入口。
function(artichoco_stage_libraries target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot stage runtime dependencies for unknown target: ${target}")
    endif()

    if(WIN32)
        # 参数顺序是 -t <目标目录> <文件...>，**不能写成 <文件...> <目标目录>**：只链静态库的
        # exe（physics_smoke / lua_vm_smoke / task_system_test / scene_duplicate_test）的
        # $<TARGET_RUNTIME_DLLS> 展开成空，COMMAND_EXPAND_LISTS 把这个空参数整个抹掉，后一种
        # 写法就只剩目标目录一个参数，cmake -E copy_if_different 判为参数不足、打一串 usage
        # 再退出 1 —— 症状是 ninja 报链接失败，而链接本身是好的。-t 形式下空列表就是「零个
        # 源文件」，是合法的空操作。
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different -t
                "$<TARGET_FILE_DIR:${target}>"
                "$<TARGET_RUNTIME_DLLS:${target}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )

        # slang.dll 在运行时 LoadLibrary 出 slang-compiler.dll，$<TARGET_RUNTIME_DLLS> 看不见
        # 这条依赖，所以手写一条。缺了它进程连加载都过不去（0xC0000135），而报错里不会提到
        # 是谁缺的 —— 这一条不能省。
        if(ARTICHOCO_SLANG_COMPILER_RUNTIME)
            add_custom_command(TARGET "${target}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${ARTICHOCO_SLANG_COMPILER_RUNTIME}"
                    "$<TARGET_FILE_DIR:${target}>"
                VERBATIM
            )
        endif()

        # CRT：见 ArtiMsvcRuntime.cmake 的发现逻辑。找不到时它是空的（那里已经警告过），
        # 这里就是空操作。Debug 构建由那边的函数跳过 —— 调试 CRT 不可再分发。
        artichoco_stage_msvc_runtime("${target}")
    else()
        # 只给**不在默认搜索路径上**的依赖目录加 rpath。CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
        # 是链接器本来就搜的目录（/usr/lib 等），给它们写 rpath 只是噪声。
        # 名单里只有 external 那两个：vendored 的（SDL3 / glm / spdlog / …）是静态库，编进产物
        # 就没有「运行时去哪找」的问题 —— 这正是把它们 vendored 的收益之一。
        foreach(_arti_runtime_library
            "${_arti_slang_library_release}"
            "${_arti_vulkan_library}"
        )
            if(NOT _arti_runtime_library)
                continue()
            endif()
            get_filename_component(_arti_runtime_dir "${_arti_runtime_library}" DIRECTORY)
            if(_arti_runtime_dir IN_LIST CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES)
                continue()
            endif()
            set_property(TARGET "${target}" APPEND PROPERTY BUILD_RPATH "${_arti_runtime_dir}")
        endforeach()
    endif()
endfunction()
