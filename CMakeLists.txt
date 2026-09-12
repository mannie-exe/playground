cmake_minimum_required(VERSION 4.4)

project(playground LANGUAGES C CXX)

include(GNUInstallDirs)

#[[
Collect build products into predictable directories inside the active build
tree.

CMake normally chooses per-target/per-generator output locations. These
variables make the project easier to reason about by putting:

- executables and runtime files in build/<preset>/bin
- shared libraries in build/<preset>/libs
- static libraries and import libraries in build/<preset>/libs
- Windows debug symbol files in build/<preset>/bin

This is build-tree organization only. It does not decide what gets copied into
dist/ during `cmake --install`; that is handled by the install() rules below.

`link_directories()` adds the generated library directory to the linker search
path. It is broad/global and usually less precise than linking named targets
with target_link_libraries(), but it can be useful while studying mixed C/C++
native builds where generated libraries land in a common folder.
]]
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(
    DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.43.1/CPM.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
    EXPECTED_HASH SHA256=1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

#[[
Keep clangd/VSCode pointed at a stable compile database path.

CMake writes compile_commands.json into the active build directory, e.g.
build/debug/compile_commands.json or build/release/compile_commands.json. That
path changes by preset/build type, while clangd is configured to look at the
workspace root. This target runs as part of the normal build and copies the
active build's compile database to ./compile_commands.json.

The copied root file is generated editor state, not source, and is ignored by
Git.
]]
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(sync_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
        COMMENT "Syncing compile_commands.json for clangd"
        VERBATIM
    )
endif()

function(playground_register_install_target target_name)
    set_property(GLOBAL APPEND PROPERTY playground_install_targets ${target_name})
endfunction()

function(playground_configure_cpp_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_compile_options(${target_name} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
endfunction()

function(playground_add_console_target target_name)
    cmake_parse_arguments(PARSE_ARGV 1 target "" "" "SOURCES;INCLUDES")

    add_executable(${target_name} ${target_SOURCES})
    playground_configure_cpp_target(${target_name})

    if(target_INCLUDES)
        target_include_directories(${target_name} PRIVATE ${target_INCLUDES})
    endif()

    playground_register_install_target(${target_name})
endfunction()

function(playground_add_sdl_target target_name)
    cmake_parse_arguments(PARSE_ARGV 1 target "" "" "SOURCES;INCLUDES")

    add_executable(${target_name} ${target_SOURCES})
    playground_configure_cpp_target(${target_name})

    if(target_INCLUDES)
        target_include_directories(${target_name} PRIVATE ${target_INCLUDES})
    endif()

    target_link_libraries(${target_name} PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf)

    playground_register_install_target(${target_name})
endfunction()

playground_add_console_target(experiment_rock_paper_scissors
    SOURCES
    experiments/rock_paper_scissors/main.cpp
    INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/rock_paper_scissors/include)

CPMAddPackage(
    URI "gh:libsdl-org/SDL#147a8ee@3.4.14"
    OPTIONS
    "SDL_SHARED ON"
    "SDL_STATIC OFF"
    "SDL_TEST_LIBRARY OFF"
    "SDL_TESTS OFF"
    "SDL_EXAMPLES OFF"
    "SDL_INSTALL OFF"
    "SDL_WERROR OFF"
)

CPMAddPackage(
    URI "gh:libsdl-org/SDL_image#bec9134@3.4.4"
    OPTIONS
    "SDLIMAGE_INSTALL OFF"
    "SDLIMAGE_SAMPLES OFF"
    "SDLIMAGE_TESTS OFF"
    "SDLIMAGE_VENDORED ON"
    "SDLIMAGE_STRICT OFF"
    "SDLIMAGE_WERROR OFF"
    "SDLIMAGE_AVIF OFF"
    "SDLIMAGE_AVIF_SAVE OFF"
    "SDLIMAGE_JXL OFF"
    "SDLIMAGE_TIF OFF"
    "SDLIMAGE_WEBP OFF"
    "SDLIMAGE_ANI OFF"
    "SDLIMAGE_GIF OFF"
    "SDLIMAGE_LBM OFF"
    "SDLIMAGE_PCX OFF"
    "SDLIMAGE_PNM OFF"
    "SDLIMAGE_SVG ON"
    "SDLIMAGE_TGA OFF"
    "SDLIMAGE_XCF OFF"
    "SDLIMAGE_XPM OFF"
    "SDLIMAGE_XV OFF"
    "SDLIMAGE_BMP ON"
    "SDLIMAGE_JPG ON"
    "SDLIMAGE_PNG ON"
    "SDLIMAGE_PNG_LIBPNG ON"
    "SDLIMAGE_QOI ON"
    "SDLIMAGE_BMP_SAVE OFF"
    "SDLIMAGE_JPG_SAVE OFF"
    "SDLIMAGE_PNG_SAVE OFF"
)

CPMAddPackage(
    URI "gh:libsdl-org/SDL_ttf#a1ce367@3.2.2"
    OPTIONS
    "SDLTTF_INSTALL OFF"
    "SDLTTF_SAMPLES OFF"
    "SDLTTF_TESTS OFF"
    "SDLTTF_VENDORED ON"
    "SDLTTF_STRICT OFF"
    "SDLTTF_WERROR OFF"
)

playground_add_sdl_target(experiment_sdl3
    SOURCES
    experiments/sdl3/src/main.cpp
    INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/experiments/sdl3/include)

playground_add_sdl_target(playground
    SOURCES
    src/main.cpp
    INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(playground_assets_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/assets)

#[[
Install means "assemble the runnable/exportable form of this project."

For this repo, install does not mean modifying the operating system or placing
files into Program Files, /usr/local, or another machine-wide location. The
install prefix from CMakePresets.json points at dist/<config>, so install means:

- copy our executables into dist/<config>/bin
- copy the SDL shared libraries they need into dist/<config>/bin on Windows
- copy shared libraries into dist/<config>/lib on Unix-like systems when that is
  the platform convention
- copy import/static libraries into dist/<config>/lib when CMake has them

Targets are CMake's named build products. `experiment_rock_paper_scissors` and
`experiment_sdl3` are executable targets created by add_executable(). SDL's CMake
build may also create shared-library targets such as SDL3-shared,
SDL3_image-shared, and SDL3_ttf-shared. If those targets exist, appending them
to playground_install_targets tells CMake to install those built SDL libraries
next to our programs.

This matters because linking and running are separate concerns. Linking records
which libraries an executable uses; installing collects the executable and the
runtime libraries into a layout that can run without changing PATH, DYLD paths,
or Linux loader configuration during normal local experimentation.
]]

get_property(playground_install_targets GLOBAL PROPERTY playground_install_targets)

foreach(shared_target IN ITEMS
    SDL3-shared
    SDL3_image-shared
    SDL3_ttf-shared)

    if(TARGET ${shared_target})
        list(APPEND playground_install_targets ${shared_target})
    endif()
endforeach()

install(
    TARGETS ${playground_install_targets}
    RUNTIME_DEPENDENCY_SET playground_runtime_deps
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

#[[
Install runtime dependencies discovered from the targets above.

`RUNTIME_DEPENDENCY_SET playground_runtime_deps` in the previous install(TARGETS)
call asks CMake to scan those targets for dynamic library dependencies. This
second install() call copies the discovered dependencies into the install tree.

The exclude regexes prevent CMake from copying operating-system libraries:

- api-ms-* and ext-ms-* are Windows API-set forwarding DLL names
- system32/System32/Windows paths are Windows system components
- /System/Library and /usr/lib are system library locations on macOS/Linux

The goal is to copy project/runtime dependencies such as SDL DLLs or shared
libraries, while leaving OS-provided libraries where the OS already provides
them.
]]
install(
    RUNTIME_DEPENDENCY_SET playground_runtime_deps
    PRE_EXCLUDE_REGEXES
    [[api-ms-]]
    [[ext-ms-]]
    POST_EXCLUDE_REGEXES
    [[.*[\\/]system32[\\/].*]]
    [[.*[\\/]System32[\\/].*]]
    [[.*[\\/]Windows[\\/].*]]
    [[^/System/Library/.*]]
    [[^/usr/lib/.*]]
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(EXISTS ${playground_assets_dir})
    install(
        DIRECTORY ${playground_assets_dir}/
        DESTINATION ${CMAKE_INSTALL_BINDIR}/assets
    )
endif()
