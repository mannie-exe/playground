# Portable application layout shared by all native desktop targets.
get_property(playground_install_targets GLOBAL PROPERTY playground_install_targets)
foreach(shared_target IN ITEMS SDL3-shared SDL3_image-shared SDL3_ttf-shared)
    if(TARGET ${shared_target})
        list(APPEND playground_install_targets ${shared_target})
    endif()
endforeach()
install(TARGETS ${playground_install_targets}
    RUNTIME_DEPENDENCY_SET playground_runtime_deps
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(PLAYGROUND_PLATFORM STREQUAL "windows")
    # Windows API-set forwarders and OS DLLs belong to the operating system.
    set(runtime_pre_excludes [[api-ms-]] [[ext-ms-]])
    set(runtime_post_excludes [[.*[\\/]system32[\\/].*]] [[.*[\\/]System32[\\/].*]]
        [[.*[\\/]Windows[\\/].*]])
    if(MSVC AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Portable archives use the toolchain's redistributable release CRT.
        # Debug CRT files are development-only and must not be redistributed.
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
        include(InstallRequiredSystemLibraries)
    endif()
elseif(PLAYGROUND_PLATFORM STREQUAL "macos")
    set(runtime_post_excludes [[^/System/Library/.*]] [[^/usr/lib/.*]])
    include(${CMAKE_CURRENT_LIST_DIR}/MoltenVK.cmake)
elseif(PLAYGROUND_PLATFORM STREQUAL "linux")
    # The host supplies libc, the ELF loader, desktop services and GPU drivers.
    # These archives retain a dependency on the build distribution's ABI.
    set(runtime_post_excludes [[^/usr/lib/.*]] [[^/usr/lib64/.*]]
        [[^/lib/.*]] [[^/lib64/.*]])
endif()
install(RUNTIME_DEPENDENCY_SET playground_runtime_deps
    PRE_EXCLUDE_REGEXES ${runtime_pre_excludes}
    POST_EXCLUDE_REGEXES ${runtime_post_excludes}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/"
    DESTINATION ${CMAKE_INSTALL_BINDIR}/assets)

# SDL's development install rules are disabled; retain its runtime notices here.
foreach(dependency IN ITEMS SDL SDL_image SDL_ttf)
    install(FILES "${${dependency}_SOURCE_DIR}/LICENSE.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME "${dependency}.txt")
endforeach()
get_directory_property(image_licenses DIRECTORY "${SDL_image_SOURCE_DIR}" DEFINITION install_license_names)
foreach(license IN LISTS image_licenses)
    get_directory_property(license_file DIRECTORY "${SDL_image_SOURCE_DIR}" DEFINITION install_license_${license})
    install(FILES "${SDL_image_SOURCE_DIR}/${license_file}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/SDL_image RENAME "${license}.txt")
endforeach()
install(FILES "${SDL_ttf_SOURCE_DIR}/external/freetype/LICENSE.TXT"
    "${SDL_ttf_SOURCE_DIR}/external/freetype/docs/FTL.TXT"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/freetype)
install(FILES "${SDL_ttf_SOURCE_DIR}/external/harfbuzz/COPYING"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/harfbuzz)
install(FILES "${SDL_ttf_SOURCE_DIR}/external/harfbuzz/src/ms-use/COPYING" RENAME harfbuzz-use.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/harfbuzz)
foreach(dependency IN ITEMS plutosvg plutovg)
    install(FILES "${SDL_ttf_SOURCE_DIR}/external/${dependency}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME "${dependency}.txt")
endforeach()
install(FILES "${SDL_ttf_SOURCE_DIR}/external/plutovg/source/FTL.TXT"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/plutovg)

if(PLAYGROUND_PLATFORM STREQUAL "macos")
    configure_file(${CMAKE_CURRENT_LIST_DIR}/InstallMacRuntime.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/InstallMacRuntime.cmake @ONLY)
    install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/InstallMacRuntime.cmake)
endif()
