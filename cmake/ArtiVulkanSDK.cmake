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

    find_library(_arti_slang_library_debug
        NAMES slangd slang
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Lib lib
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_slang_runtime_release
        NAMES slang.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )
    find_file(_arti_slang_runtime_debug
        NAMES slangd.dll slang.dll
        HINTS ${_arti_vulkan_sdk_prefixes}
        PATH_SUFFIXES Bin bin
        NO_DEFAULT_PATH
        NO_CACHE
        REQUIRED
    )

    add_library(arti_sdk_slang SHARED IMPORTED GLOBAL)
    set_target_properties(arti_sdk_slang PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_IMPLIB "${_arti_slang_library_release}"
        IMPORTED_LOCATION "${_arti_slang_runtime_release}"
        IMPORTED_IMPLIB_DEBUG "${_arti_slang_library_debug}"
        IMPORTED_LOCATION_DEBUG "${_arti_slang_runtime_debug}"
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
