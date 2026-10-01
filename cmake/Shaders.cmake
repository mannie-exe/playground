# Normal builds compile the pinned tool automatically. Reuse an existing tool
# only to avoid rebuilding its substantial DXC/LLVM dependencies.
set(PLAYGROUND_SHADERCROSS_EXECUTABLE "" CACHE FILEPATH "Existing shadercross executable (empty builds the pinned tool)")
mark_as_advanced(PLAYGROUND_SHADERCROSS_EXECUTABLE)
if(NOT PLAYGROUND_GPU)
    message(STATUS "GPU shaders disabled for software-only development")
    return()
endif()
if(NOT PLAYGROUND_SHADERCROSS_EXECUTABLE)
    # No tags/releases are published upstream. Pin the reviewed source revision.
    CPMAddPackage(
        NAME SDL_shadercross
        GITHUB_REPOSITORY libsdl-org/SDL_shadercross
        GIT_TAG 1ff05bec573988a98ef9e0260b4da44f512b8367
        DOWNLOAD_ONLY YES
    )
    # Patch before configuration, including CPM local-source overrides.
    if(APPLE)
        include(${CMAKE_CURRENT_LIST_DIR}/patches/ShadercrossMac.cmake)
        playground_patch_shadercross_macos("${SDL_shadercross_SOURCE_DIR}")
    endif()
    function(playground_add_shadercross)
        set(SDLSHADERCROSS_VENDORED ON)
        set(SDLSHADERCROSS_DXC ON)
        set(SDLSHADERCROSS_SHARED ON)
        set(SDLSHADERCROSS_STATIC OFF)
        set(SDLSHADERCROSS_SPIRVCROSS_SHARED ON)
        set(SDLSHADERCROSS_CLI ON)
        set(SDLSHADERCROSS_TESTS OFF)
        set(SDLSHADERCROSS_INSTALL OFF)
        set(SDLSHADERCROSS_WERROR OFF)
        set(SPIRV_SKIP_TESTS ON)
        set(SPIRV_SKIP_EXECUTABLES ON)
        add_subdirectory("${SDL_shadercross_SOURCE_DIR}" "${SDL_shadercross_BINARY_DIR}" EXCLUDE_FROM_ALL)
    endfunction()
    playground_add_shadercross()
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
else()
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
    if(IS_ABSOLUTE "${source}")
        set(input "${source}")
    else()
        set(input "${PROJECT_SOURCE_DIR}/${source}")
    endif()
    get_filename_component(input_dir "${input}" DIRECTORY)
    set(output_dir "${PROJECT_BINARY_DIR}/shaders")
    set(outputs)
    foreach(format IN ITEMS SPIRV JSON)
        string(TOLOWER "${format}" extension)
        set(output "${output_dir}/${name}.${extension}")
        add_custom_command(OUTPUT "${output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
            COMMAND "${playground_shader_tool}" "${input}"
                -I "${input_dir}" -s HLSL -d "${format}" -t "${stage}" -e main -o "${output}"
            DEPENDS "${input}" ${shader_UNPARSED_ARGUMENTS} "${playground_shader_tool}" ${playground_shader_tool_dependency}
            COMMENT "Compiling ${name} to ${format}"
            VERBATIM)
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
playground_add_shader(scene_pbr_vertex shaders/scene_pbr.vert.hlsl vertex)
playground_add_shader(scene_pbr_fragment shaders/scene_pbr.frag.hlsl fragment)
playground_add_shader(scene_tone_vertex shaders/scene_tone.vert.hlsl vertex)
playground_add_shader(scene_tone_fragment shaders/scene_tone.frag.hlsl fragment)
playground_add_shader(paint_vertex shaders/paint.vert.hlsl vertex)
playground_add_shader(paint_fragment shaders/paint.frag.hlsl fragment)
playground_add_shader(paint_rect_fragment shaders/paint_rect.frag.hlsl fragment
    "${PROJECT_SOURCE_DIR}/shaders/paint.frag.hlsl")
playground_add_shader(present_fragment shaders/present.frag.hlsl fragment
    "${PROJECT_SOURCE_DIR}/shaders/paint_rect.frag.hlsl"
    "${PROJECT_SOURCE_DIR}/shaders/paint.frag.hlsl")
target_compile_definitions(playground_sdl PRIVATE
    PLAYGROUND_SHADER_DIRECTORY="${PROJECT_BINARY_DIR}/shaders"
    PLAYGROUND_SHADER_SPIRV=1)
add_dependencies(playground_sdl playground_shaders)
message(STATUS "GPU shaders enabled: SPIRV")
