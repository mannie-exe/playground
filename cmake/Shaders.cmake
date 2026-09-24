# Offline tools are optional: software-only builds never fetch DXC/LLVM.
# A host executable may be supplied when cross-compiling the application.
option(PLAYGROUND_BUILD_SHADERCROSS "Build pinned shadercross and vendored compilers" OFF)
set(PLAYGROUND_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH "Host shadercross executable (including its runtime dependencies)")
set(PLAYGROUND_SHADER_FORMATS "SPIRV" CACHE STRING "Packaged shader format (Vulkan only)")
set(PLAYGROUND_DXC_EXECUTABLE "" CACHE FILEPATH "Optional host DXC producing SPIR-V")
foreach(format IN LISTS PLAYGROUND_SHADER_FORMATS)
    if(NOT format STREQUAL "SPIRV")
        message(FATAL_ERROR "Only Vulkan/SPIRV is supported; set PLAYGROUND_SHADER_FORMATS=SPIRV")
    endif()
endforeach()

if(PLAYGROUND_BUILD_SHADERCROSS AND PLAYGROUND_SHADERCROSS_EXECUTABLE)
    message(FATAL_ERROR "Choose a shadercross source build OR a host executable")
endif()
if(PLAYGROUND_DXC_EXECUTABLE)
    if(PLAYGROUND_BUILD_SHADERCROSS OR PLAYGROUND_SHADERCROSS_EXECUTABLE)
        message(FATAL_ERROR "Choose DXC or shadercross, not both")
    endif()
    if(NOT EXISTS "${PLAYGROUND_DXC_EXECUTABLE}" OR IS_DIRECTORY "${PLAYGROUND_DXC_EXECUTABLE}")
        message(FATAL_ERROR "PLAYGROUND_DXC_EXECUTABLE does not exist")
    endif()
endif()
if(PLAYGROUND_BUILD_SHADERCROSS)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Cross builds require PLAYGROUND_SHADERCROSS_EXECUTABLE for the host")
    endif()
    # No tags/releases are published upstream. Pin the reviewed source revision.
    CPMAddPackage(
        NAME SDL_shadercross
        GITHUB_REPOSITORY libsdl-org/SDL_shadercross
        GIT_TAG 1ff05bec573988a98ef9e0260b4da44f512b8367
        OPTIONS
        "SDLSHADERCROSS_VENDORED ON"
        "SDLSHADERCROSS_DXC ON"
        "SDLSHADERCROSS_SHARED ON"
        "SDLSHADERCROSS_STATIC OFF"
        "SDLSHADERCROSS_SPIRVCROSS_SHARED ON"
        "SDLSHADERCROSS_CLI ON"
        "SDLSHADERCROSS_TESTS OFF"
        "SDLSHADERCROSS_INSTALL OFF"
        "SDLSHADERCROSS_WERROR OFF"
        "SPIRV_SKIP_TESTS ON"
        "SPIRV_SKIP_EXECUTABLES ON"
    )
    set(playground_shader_tool "$<TARGET_FILE:shadercross>")
    set(playground_shader_tool_dependency shadercross)
    if(WIN32)
        add_custom_target(playground_shader_tool_runtime
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:shadercross> $<TARGET_FILE_DIR:shadercross>
            DEPENDS shadercross
            COMMAND_EXPAND_LISTS VERBATIM)
        set(playground_shader_tool_dependency playground_shader_tool_runtime)
        # DXIL is loaded by DXC, not necessarily visible in runtime-link scanning.
        if(TARGET dxildll)
            add_custom_target(playground_shader_dxc_runtime
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:dxildll> $<TARGET_FILE_DIR:shadercross>
                DEPENDS shadercross dxildll
                VERBATIM)
            add_dependencies(playground_shader_tool_runtime playground_shader_dxc_runtime)
        endif()
    endif()
elseif(PLAYGROUND_SHADERCROSS_EXECUTABLE)
    if(NOT EXISTS "${PLAYGROUND_SHADERCROSS_EXECUTABLE}" OR IS_DIRECTORY "${PLAYGROUND_SHADERCROSS_EXECUTABLE}")
        message(FATAL_ERROR "PLAYGROUND_SHADERCROSS_EXECUTABLE does not exist")
    endif()
    set(playground_shader_tool "${PLAYGROUND_SHADERCROSS_EXECUTABLE}")
    set(playground_shader_tool_dependency "${PLAYGROUND_SHADERCROSS_EXECUTABLE}")
endif()

function(playground_add_shader name source stage)
    cmake_parse_arguments(PARSE_ARGV 3 shader "NO_INSTALL" "" "")
    if(NOT stage MATCHES "^(vertex|fragment|compute)$")
        message(FATAL_ERROR "Invalid shader stage: ${stage}")
    endif()
    if(NOT playground_shader_tool AND NOT PLAYGROUND_DXC_EXECUTABLE)
        return()
    endif()
    if(IS_ABSOLUTE "${source}")
        set(input "${source}")
    else()
        set(input "${PROJECT_SOURCE_DIR}/${source}")
    endif()
    set(output_dir "${PROJECT_BINARY_DIR}/shaders")
    set(outputs)
    set(formats ${PLAYGROUND_SHADER_FORMATS} JSON)
    list(REMOVE_DUPLICATES formats)
    foreach(format IN LISTS formats)
        if(PLAYGROUND_DXC_EXECUTABLE AND NOT playground_shader_tool AND format STREQUAL "JSON")
            continue()
        endif()
        string(TOLOWER "${format}" extension)
        set(output "${output_dir}/${name}.${extension}")
        if(PLAYGROUND_DXC_EXECUTABLE AND NOT playground_shader_tool)
            if(stage STREQUAL "vertex")
                set(profile vs_6_0)
            elseif(stage STREQUAL "fragment")
                set(profile ps_6_0)
            else()
                set(profile cs_6_0)
            endif()
            set(extra -spirv -fspv-target-env=vulkan1.0)
            add_custom_command(OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                COMMAND "${PLAYGROUND_DXC_EXECUTABLE}" "${input}" -T ${profile} -E main -Fo "${output}" ${extra}
                DEPENDS "${input}" ${shader_UNPARSED_ARGUMENTS} "${PLAYGROUND_DXC_EXECUTABLE}" VERBATIM)
        else()
            add_custom_command(OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
                COMMAND "${playground_shader_tool}" "${input}"
                    -s HLSL -d "${format}" -t "${stage}" -e main -o "${output}"
                DEPENDS "${input}" ${shader_UNPARSED_ARGUMENTS} "${playground_shader_tool}" ${playground_shader_tool_dependency}
                COMMENT "Compiling ${name} to ${format}"
                VERBATIM)
        endif()
        list(APPEND outputs "${output}")
    endforeach()
    add_custom_target(playground_shader_${name} DEPENDS ${outputs})
    add_dependencies(playground_shaders playground_shader_${name})
    if(NOT shader_NO_INSTALL)
        install(FILES ${outputs} DESTINATION ${CMAKE_INSTALL_BINDIR}/assets/shaders)
    endif()
endfunction()

add_custom_target(playground_shaders)
playground_add_shader(scene_unlit_vertex shaders/scene_unlit.vert.hlsl vertex)
playground_add_shader(scene_unlit_fragment shaders/scene_unlit.frag.hlsl fragment)
playground_add_shader(paint_vertex shaders/paint.vert.hlsl vertex)
playground_add_shader(paint_fragment shaders/paint.frag.hlsl fragment)
if(playground_shader_tool OR PLAYGROUND_DXC_EXECUTABLE)
    target_compile_definitions(playground_sdl PRIVATE PLAYGROUND_SHADER_DIRECTORY="${PROJECT_BINARY_DIR}/shaders")
    foreach(format IN LISTS PLAYGROUND_SHADER_FORMATS)
        target_compile_definitions(playground_sdl PRIVATE PLAYGROUND_SHADER_${format}=1)
    endforeach()
    add_dependencies(playground_sdl playground_shaders)
    message(STATUS "Offline shaders enabled: ${PLAYGROUND_SHADER_FORMATS}")
else()
    message(STATUS "Offline shaders disabled (software rendering needs no shader compiler)")
endif()
