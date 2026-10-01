# SDL loads Vulkan dynamically, so the runtime dependency scanner cannot find it.
# Software-only configurations do not need this runtime.
if(NOT PLAYGROUND_GPU)
    return()
endif()
set(playground_moltenvk_version 1.4.2)
CPMAddPackage(NAME moltenvk_binary
    URL https://github.com/KhronosGroup/MoltenVK/releases/download/v${playground_moltenvk_version}/MoltenVK-macos.tar
    URL_HASH SHA256=f95765a6229cb7b915990a2890ce12ebe36a730b021545d3d52ae69ce4c4024e
    DOWNLOAD_ONLY YES)
find_program(playground_lipo lipo REQUIRED)
find_program(playground_codesign codesign REQUIRED)
if(PLAYGROUND_ARCHITECTURE STREQUAL "amd64")
    set(moltenvk_arch x86_64)
else()
    set(moltenvk_arch arm64)
endif()
# SDL's Cocoa loader already searches @executable_path/../Frameworks. Use that
# convention in both the build tree and the installed portable directory.
set(moltenvk_runtime "${CMAKE_CURRENT_BINARY_DIR}/Frameworks/libMoltenVK.dylib")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Frameworks")
execute_process(COMMAND "${playground_lipo}"
    "${moltenvk_binary_SOURCE_DIR}/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib"
    -thin ${moltenvk_arch} -output "${moltenvk_runtime}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${playground_codesign}" --force --sign - "${moltenvk_runtime}"
    COMMAND_ERROR_IS_FATAL ANY)
install(FILES "${moltenvk_runtime}" DESTINATION ${CMAKE_INSTALL_BINDIR}/../Frameworks)
install(FILES "${moltenvk_binary_SOURCE_DIR}/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME moltenvk.txt)
install(FILES "${PROJECT_SOURCE_DIR}/docs/licenses/MOLTENVK-THIRD-PARTY.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK-notice.txt"
    "MoltenVK ${playground_moltenvk_version}, KhronosGroup/MoltenVK, Apache-2.0.\n"
    "Copyright (c) 2015-2026 The Brenwill Workshop Ltd.\n"
    "The upstream macOS public-API binary is reduced to ${moltenvk_arch}; install names and code signatures are adjusted for this distribution.\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/MoltenVK-notice.txt"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses)
