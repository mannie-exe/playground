cmake_minimum_required(VERSION 4.4)

# Set before compiler detection so every target shares the supported macOS floor.
if(CMAKE_HOST_APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "Minimum supported macOS version" FORCE)
    elseif(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "26.0")
        message(FATAL_ERROR "playground requires macOS 26 or later")
    endif()
endif()

set(PLAYGROUND_VERSION "0.0.0" CACHE STRING "Application version (major.minor.patch; 0.0.0 denotes development)")
if(NOT PLAYGROUND_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "PLAYGROUND_VERSION must be major.minor.patch")
endif()
project(playground VERSION ${PLAYGROUND_VERSION} LANGUAGES C CXX)
include(cmake/Platform.cmake)
if(APPLE)
    # Enable in the common parent before SDL adds Objective-C/Objective-C++
    # sources; enabling from a dependency first loses rule variables in siblings.
    enable_language(OBJC OBJCXX)
endif()

option(PLAYGROUND_BUILD_TESTS "Build focused UI and supporting-module tests" ON)
option(PLAYGROUND_GPU "Build GPU shaders and include hardware tests (disable for software-only development)" ON)

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

Dependency search paths come from the linked CMake targets below rather than
global link_directories().
]]
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/Dependencies.cmake)
include(cmake/Accessibility.cmake)

#[[
Keep clangd/VSCode pointed at a stable compile database path.

CMake writes compile_commands.json into the active build directory, e.g.
build/debug/compile_commands.json or build/release/compile_commands.json. That
path changes by preset/build type, while clangd is configured to look at the
workspace root. Each project build generates an editor database with explicit
header entries from the owning implementation's command or the application
command. The build directory's original database is untouched.

The copied root file is generated editor state, not source, and is ignored by
Git.
]]
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(sync_compile_commands ALL
        COMMAND ${CMAKE_COMMAND}
        -DPROJECT_ROOT=${CMAKE_SOURCE_DIR}
        -DBUILD_ROOT=${CMAKE_BINARY_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateEditorCommands.cmake
        COMMENT "Generating compile_commands.json with explicit header entries"
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
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
    if(TARGET sync_compile_commands)
        add_dependencies(${target_name} sync_compile_commands)
    endif()
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

CPMAddPackage(
    URI "gh:libsdl-org/SDL#fa2c02bb6e21974a89ea9824bc53c9932abe5f9c@3.4.16"
    OPTIONS
    "SDL_SHARED ON"
    "SDL_STATIC OFF"
    "SDL_TEST_LIBRARY OFF"
    "SDL_TESTS OFF"
    "SDL_EXAMPLES OFF"
    "SDL_INSTALL OFF"
    "SDL_WERROR OFF"
    "SDL_GPU ON"
)

include(cmake/SDLTimestamps.cmake)
playground_apply_sdl_timestamps("${SDL_SOURCE_DIR}")

CPMAddPackage(
    URI "gh:libsdl-org/SDL_image#f661fa1ad24ab1b81e43662532f9a6a9fcf67ea6@3.4.6"
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
    "SDLIMAGE_PNG_SHARED OFF"
    "SDLIMAGE_QOI ON"
    "SDLIMAGE_BMP_SAVE OFF"
    "SDLIMAGE_JPG_SAVE OFF"
    "SDLIMAGE_PNG_SAVE OFF"
)

# FreeType is mandatory in this SDL_ttf version; HarfBuzz shapes glyph runs,
# while SDL_ttf's GPU text engine owns atlas generation for the GPU renderer.
# Bitmap emoji use PNG-compressed glyphs. Reuse SDL_image's managed libraries
# through FindPNG/FindZLIB instead of discovering host installations.
set(FT_DISABLE_PNG OFF CACHE BOOL "Enable bitmap color glyphs" FORCE)
set(FT_REQUIRE_PNG ON CACHE BOOL "Require bitmap color glyph support" FORCE)
set(PNG_LIBRARY PNG::PNG)
set(PNG_PNG_INCLUDE_DIR "${SDL_image_SOURCE_DIR}/external/libpng")
set(ZLIB_LIBRARY ZLIB::ZLIB)
CPMAddPackage(
    URI "gh:libsdl-org/SDL_ttf#a1ce367@3.2.2"
    OPTIONS
    "SDLTTF_INSTALL OFF"
    "SDLTTF_SAMPLES OFF"
    "SDLTTF_TESTS OFF"
    "SDLTTF_VENDORED ON"
    "SDLTTF_STRICT OFF"
    "SDLTTF_WERROR OFF"
    "SDLTTF_HARFBUZZ ON"
    "SDLTTF_PLUTOSVG ON"
)

include(cmake/patches/SDLTTFText.cmake)
playground_patch_sdl_ttf("${SDL_ttf_SOURCE_DIR}")

CPMAddPackage(
    URI "gh:JuliaStrings/utf8proc#79cdd5ab40c79afa559f689ffc13b76812dee1ac@2.12.0"
    OPTIONS "BUILD_SHARED_LIBS OFF" "UTF8PROC_INSTALL OFF" "UTF8PROC_ENABLE_TESTING OFF"
)
CPMAddPackage(
    URI "gh:zeux/pugixml@1.16"
    OPTIONS "BUILD_SHARED_LIBS OFF" "PUGIXML_INSTALL OFF" "PUGIXML_BUILD_TESTS OFF"
)

CPMAddPackage(
    URI "gh:nucleic/kiwi#166899b656d8caa3b2444f5ab2464e55a50b9554@1.5.1"
    DOWNLOAD_ONLY YES
)
CPMAddPackage(
    URI "gh:marzer/tomlplusplus@3.4.0"
    OPTIONS "TOMLPLUSPLUS_BUILD_TESTS OFF" "TOMLPLUSPLUS_BUILD_EXAMPLES OFF"
)
CPMAddPackage(
    URI "gh:jkuhlmann/cgltf#360db1a95480fe102ae9c69b27c5d101167ff5ba@1.15"
    DOWNLOAD_ONLY YES
)
CPMAddPackage(
    NAME spirv_reflect
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Reflect
    # vulkan-sdk-1.4.357.0
    GIT_TAG 9137a7e529e4e911a4dbd63f1ce71e4b3e8f4cfc
    OPTIONS
    "SPIRV_REFLECT_STATIC_LIB ON"
    "SPIRV_REFLECT_EXECUTABLE OFF"
    "SPIRV_REFLECT_SHARED_LIB OFF"
    "SPIRV_REFLECT_BUILD_TESTS OFF"
    "SPIRV_REFLECT_EXAMPLES OFF"
    "SPIRV_REFLECT_INSTALL OFF"
)
# These upstream projects publish no release tags; retain reviewed commits.
CPMAddPackage(NAME mikktspace GITHUB_REPOSITORY mmikk/MikkTSpace
    GIT_TAG 3e895b49d05ea07e4c2133156cfa94369e19e409 DOWNLOAD_ONLY YES)
CPMAddPackage(NAME stb GITHUB_REPOSITORY nothings/stb
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20 DOWNLOAD_ONLY YES)
CPMAddPackage(
    URI "gh:zeux/meshoptimizer#9e1f07b159d3cb777f1c67ed31fc11fd117986f4@1.3"
    OPTIONS "MESHOPT_BUILD_DEMO OFF" "MESHOPT_BUILD_GLTFPACK OFF" "MESHOPT_BUILD_SHARED_LIBS OFF")
CPMAddPackage(NAME ktx GITHUB_REPOSITORY KhronosGroup/KTX-Software
    # v4.4.2
    GIT_TAG 936b655d10fe75f900967f524ba31005bebcbb47
    GIT_SUBMODULES "" DOWNLOAD_ONLY YES)
# Only the reader/transcoder is linked; dependency tools, tests and install
# rules are excluded from our application build/package.
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE STRING "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
# 4.4.2's generic reader references KTX1 symbols even in a KTX2-only static
# build. Compile that internal reader; our public decoder still accepts KTX2 only.
set(KTX_FEATURE_KTX1 ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_KTX2 ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(BASISU_SUPPORT_OPENCL OFF CACHE BOOL "" FORCE)
function(playground_add_ktx)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(${ktx_SOURCE_DIR} ${ktx_BINARY_DIR} EXCLUDE_FROM_ALL)
endfunction()
playground_add_ktx()
install(FILES ${meshoptimizer_SOURCE_DIR}/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME meshoptimizer.txt)
install(DIRECTORY ${ktx_SOURCE_DIR}/LICENSES/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/ktx)
install(FILES ${ktx_SOURCE_DIR}/LICENSE.md ${ktx_SOURCE_DIR}/NOTICE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/ktx)
add_library(playground_mikktspace STATIC ${mikktspace_SOURCE_DIR}/mikktspace.c)
target_include_directories(playground_mikktspace SYSTEM PUBLIC ${mikktspace_SOURCE_DIR})
install(FILES ${mikktspace_SOURCE_DIR}/mikktspace.h
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME mikktspace.txt)
install(FILES ${stb_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME stb.txt)
include(cmake/Modules.cmake)
include(cmake/Shaders.cmake)

install(FILES ${tomlplusplus_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME tomlplusplus.txt)
install(FILES config/project.toml DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${utf8proc_SOURCE_DIR}/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME utf8proc.txt)
install(FILES ${pugixml_SOURCE_DIR}/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME pugixml.txt)
install(FILES ${kiwi_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME kiwi.txt)
install(FILES ${cgltf_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME cgltf.txt)
install(FILES ${spirv_reflect_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME spirv-reflect.txt)
install(FILES docs/licenses/UNICODE.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses)

add_library(playground_app STATIC
    src/app/AppHost.cpp
    src/app/AppContext.cpp
    src/app/UISession.cpp
    src/demo2d/Demo2DUI.cpp
    src/demo3d/Demo3DApp.cpp
    src/menu/MenuApp.cpp
    src/menu/MenuUI.cpp
    src/minesweeper/MinesweeperCell.cpp
    src/minesweeper/MinesweeperApp.cpp
    src/minesweeper/DifficultyUI.cpp
    src/minesweeper/MinesweeperModel.cpp
    src/minesweeper/MinesweeperGrid.cpp
    src/minesweeper/MinesweeperUI.cpp)
playground_configure_cpp_target(playground_app)
target_link_libraries(playground_app PUBLIC playground_sdl playground_constraints)
playground_add_sdl_target(playground SOURCES src/main.cpp)
target_link_libraries(playground PRIVATE playground_app)

# Compile the guide's additive snippets without adding an example program.
set(playground_guide_check ${CMAKE_CURRENT_BINARY_DIR}/generated/UIGuideCheck.cpp)
add_custom_command(OUTPUT ${playground_guide_check}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/docs/ui/GUIDE.md
        -DOUTPUT=${playground_guide_check}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGuideCheck.cmake
    DEPENDS docs/ui/GUIDE.md cmake/GenerateGuideCheck.cmake
    VERBATIM)
add_library(playground_docs_check OBJECT EXCLUDE_FROM_ALL ${playground_guide_check})
playground_configure_cpp_target(playground_docs_check)
target_link_libraries(playground_docs_check PRIVATE playground_sdl playground_constraints)

if(PLAYGROUND_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Install.cmake)
include(cmake/Package.cmake)
