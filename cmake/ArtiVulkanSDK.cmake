include_guard(GLOBAL)

set(ARTI_VULKAN_SDK "" CACHE PATH "Vulkan SDK installation prefix")

if(NOT ARTI_VULKAN_SDK AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" _arti_vulkan_sdk_from_environment)
    set(ARTI_VULKAN_SDK "${_arti_vulkan_sdk_from_environment}" CACHE PATH
        "Vulkan SDK installation prefix" FORCE)
endif()

if(NOT ARTI_VULKAN_SDK)
    message(FATAL_ERROR
        "Vulkan SDK was not found. Set VULKAN_SDK or configure with "
        "-DARTI_VULKAN_SDK=<path-to-sdk>.")
endif()

file(REAL_PATH "${ARTI_VULKAN_SDK}" _arti_vulkan_sdk_root)
if(NOT IS_DIRECTORY "${_arti_vulkan_sdk_root}")
    message(FATAL_ERROR "ARTI_VULKAN_SDK does not exist: ${ARTI_VULKAN_SDK}")
endif()

set(_arti_vulkan_sdk_prefixes
    "${_arti_vulkan_sdk_root}"
    "${_arti_vulkan_sdk_root}/x86_64"
    "${_arti_vulkan_sdk_root}/aarch64"
    "${_arti_vulkan_sdk_root}/arm64"
    "${_arti_vulkan_sdk_root}/macOS"
)

find_path(_arti_vulkan_include_dir
    NAMES vulkan/vulkan.h
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Include include
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_library(_arti_vulkan_library
    NAMES vulkan-1 vulkan
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Lib lib
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_path(_arti_glm_include_dir
    NAMES glm/glm.hpp
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Include include
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_path(_arti_sdl3_include_dir
    NAMES SDL3/SDL.h
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Include include
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_library(_arti_sdl3_library_release
    NAMES SDL3
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Lib lib
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_path(_arti_slang_include_dir
    NAMES slang/slang.h slang.h
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Include include
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_library(_arti_slang_library_release
    NAMES slang
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Lib lib
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

find_program(_arti_slangc_executable
    NAMES slangc slangc.exe
    HINTS ${_arti_vulkan_sdk_prefixes}
    PATH_SUFFIXES Bin bin
    NO_DEFAULT_PATH
    NO_CACHE
    REQUIRED
)

set(Vulkan_INCLUDE_DIR "${_arti_vulkan_include_dir}" CACHE PATH "Vulkan include directory" FORCE)
set(Vulkan_LIBRARY "${_arti_vulkan_library}" CACHE FILEPATH "Vulkan loader library" FORCE)
find_package(Vulkan 1.4 REQUIRED)

add_library(arti_sdk_vulkan INTERFACE)
add_library(ArtiSDK::Vulkan ALIAS arti_sdk_vulkan)
target_link_libraries(arti_sdk_vulkan INTERFACE Vulkan::Vulkan)
if(APPLE)
    target_compile_definitions(arti_sdk_vulkan INTERFACE VK_ENABLE_BETA_EXTENSIONS)
endif()

add_library(arti_sdk_glm INTERFACE)
add_library(ArtiSDK::GLM ALIAS arti_sdk_glm)
target_include_directories(arti_sdk_glm SYSTEM INTERFACE "${_arti_glm_include_dir}")

if(WIN32)
    find_library(_arti_sdl3_library_debug
        NAMES SDL3d SDL3
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Lib lib
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_sdl3_runtime_release
        NAMES SDL3.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_sdl3_runtime_debug
        NAMES SDL3d.dll SDL3.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
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

    # Slang 上游只发一份 DLL，没有 debug / release 之分。SDK 的 Lib 下确实有 slangd.lib，
    # 但那是 language server（slangd.exe 的伴生件）的导入库，**内嵌的 DLL 名同样是 slang.dll**。
    # 以前这里 debug 配置链 slangd.lib、却把 slangd.dll 当运行时依赖，两边对不上：staging
    # 拷过去的文件 exe 根本不加载，真正需要的 slang.dll 一个都不拷。所以两个配置统一。
    find_file(_arti_slang_runtime_release
        NAMES slang.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )

    # slang.dll 只有几十 KB，是个转发器 —— 实现在 slang-compiler.dll（20 MB 量级）里，由它在
    # 运行时 LoadLibrary 加载。CMake 的 $<TARGET_RUNTIME_DLLS> 只跟 target 依赖图，看不见
    # 动态加载，所以 artichoco_stage_vulkan_sdk_runtime() 里必须显式拷这一个。
    #
    # 只有它。slang-glslang / slang-rt / slang-glsl-module 实测都不需要（走 SPIR-V 直出，
    # 不过 glslang），三个加起来 30 MB+，别顺手都拷进去。
    find_file(_arti_slang_compiler_runtime
        NAMES slang-compiler.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
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
    add_library(arti_sdk_sdl3 UNKNOWN IMPORTED GLOBAL)
    set_target_properties(arti_sdk_sdl3 PROPERTIES
        IMPORTED_LOCATION "${_arti_sdl3_library_release}"
    )

    add_library(arti_sdk_slang UNKNOWN IMPORTED GLOBAL)
    set_target_properties(arti_sdk_slang PROPERTIES
        IMPORTED_LOCATION "${_arti_slang_library_release}"
    )
endif()

add_library(ArtiSDK::SDL3 ALIAS arti_sdk_sdl3)
set_target_properties(arti_sdk_sdl3 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_arti_sdl3_include_dir}"
)

add_library(ArtiSDK::Slang ALIAS arti_sdk_slang)
set_target_properties(arti_sdk_slang PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${_arti_slang_include_dir}"
)

add_executable(arti_sdk_slangc IMPORTED GLOBAL)
add_executable(ArtiSDK::SlangC ALIAS arti_sdk_slangc)
set_target_properties(arti_sdk_slangc PROPERTIES
    IMPORTED_LOCATION "${_arti_slangc_executable}"
)

function(artichoco_stage_vulkan_sdk_runtime target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot stage runtime dependencies for unknown target: ${target}")
    endif()

    if(WIN32)
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${target}>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        # slang.dll 在运行时 LoadLibrary 出 slang-compiler.dll，$<TARGET_RUNTIME_DLLS> 看不见
        # 这条依赖，所以手写一条。缺了它进程连加载都过不去（0xC0000135），而报错里不会提到
        # 是谁缺的 —— 这一条不能省。
        #
        # 只有链了 slang 的 target 才需要，但多拷一个文件的代价远小于「哪个 target 链了 slang」
        # 这个判断在依赖图变化时失效的代价。
        if(ARTICHOCO_SLANG_COMPILER_RUNTIME)
            add_custom_command(TARGET "${target}" POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${ARTICHOCO_SLANG_COMPILER_RUNTIME}"
                    "$<TARGET_FILE_DIR:${target}>"
                VERBATIM
            )
        endif()
    else()
        get_filename_component(_arti_sdk_runtime_dir "${_arti_sdl3_library_release}" DIRECTORY)
        set_property(TARGET "${target}" APPEND PROPERTY BUILD_RPATH "${_arti_sdk_runtime_dir}")
    endif()
endfunction()

message(STATUS "ArtiChoco Vulkan SDK: ${_arti_vulkan_sdk_root}")
message(STATUS "  Vulkan: ${Vulkan_VERSION}")
message(STATUS "  SDL3:   ${_arti_sdl3_library_release}")
message(STATUS "  Slang:  ${_arti_slang_library_release}")
message(STATUS "  slangc: ${_arti_slangc_executable}")
