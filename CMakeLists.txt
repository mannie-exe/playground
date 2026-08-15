cmake_minimum_required(VERSION 4.4)

project(tetrass LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/libs)
link_directories(${CMAKE_BINARY_DIR}/libs)

file(
    DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.43.1/CPM.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
    EXPECTED_HASH SHA256=1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

add_executable(rock_paper_scissors experiments/rock_paper_scissors.cpp)
set_target_properties(rock_paper_scissors PROPERTIES CXX_STANDARD 23)
target_compile_options(rock_paper_scissors PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
target_include_directories(rock_paper_scissors PRIVATE include)

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
        "SDLIMAGE_SVG OFF"
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

add_executable(sdl3_study studyplan_dev/sdl3/src/main.cpp)
set_target_properties(sdl3_study PROPERTIES CXX_STANDARD 23)
target_compile_options(sdl3_study PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
target_include_directories(sdl3_study PRIVATE include)
target_link_libraries(sdl3_study PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf)

set(tetrass_install_targets
    rock_paper_scissors
    sdl3_study
)

foreach(shared_target IN ITEMS SDL3-shared SDL3_image-shared SDL3_ttf-shared)
    if(TARGET ${shared_target})
        list(APPEND tetrass_install_targets ${shared_target})
    endif()
endforeach()

install(
    TARGETS ${tetrass_install_targets}
    RUNTIME_DEPENDENCY_SET tetrass_runtime_deps
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    RUNTIME_DEPENDENCY_SET tetrass_runtime_deps
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
