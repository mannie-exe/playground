# Keep Unicode behavior independent of the developer's system ICU installation.
if(WIN32)
    if(NOT MSVC)
        message(FATAL_ERROR "The pinned Windows ICU binaries require an MSVC-compatible toolchain")
    endif()
    if(PLAYGROUND_ARCHITECTURE STREQUAL "arm64")
        set(icu_arch WinARM64)
        set(icu_directory_suffix ARM64)
        set(icu_hash 34fedefb5aa7e3a31e77a8c4befd33b0020c3cc8056ec7d73550f2eeb40b3d85)
    else()
        set(icu_arch Win64)
        set(icu_directory_suffix 64)
        set(icu_hash 446b671f9437227daa79e221d4521d75793f9ecd65ac44c06e34dd848f201ac2)
    endif()
    CPMAddPackage(NAME icu_binary
        URL https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-${icu_arch}-MSVC2022.zip
        URL_HASH SHA256=${icu_hash} DOWNLOAD_ONLY YES)
    foreach(component IN ITEMS uc in dt)
        find_library(icu_${component}_library NAMES icu${component}
            PATHS "${icu_binary_SOURCE_DIR}/lib${icu_directory_suffix}"
            NO_DEFAULT_PATH NO_CACHE REQUIRED)
        find_file(icu_${component}_dll NAMES icu${component}78.dll
            PATHS "${icu_binary_SOURCE_DIR}/bin${icu_directory_suffix}"
            NO_DEFAULT_PATH NO_CACHE REQUIRED)
    endforeach()
    foreach(component IN ITEMS uc i18n data)
        if(component STREQUAL "i18n")
            set(binary in)
        elseif(component STREQUAL "data")
            set(binary dt)
        else()
            set(binary uc)
        endif()
        add_library(ICU::${component} SHARED IMPORTED GLOBAL)
        set_target_properties(ICU::${component} PROPERTIES
            IMPORTED_IMPLIB "${icu_${binary}_library}"
            IMPORTED_LOCATION "${icu_${binary}_dll}"
            INTERFACE_INCLUDE_DIRECTORIES "${icu_binary_SOURCE_DIR}/include")
        file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        file(COPY "${icu_${binary}_dll}" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        install(FILES "${icu_${binary}_dll}" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endforeach()
    set(icu_license "${icu_binary_SOURCE_DIR}/LICENSE")
else()
    CPMAddPackage(NAME icu_source
        URL https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz
        URL_HASH SHA256=3a2e7a47604ba702f345878308e6fefeca612ee895cf4a5f222e7955fabfe0c0
        DOWNLOAD_ONLY YES)
    include(ExternalProject)
    find_program(icu_make NAMES gmake make REQUIRED)
    set(icu_prefix "${CMAKE_CURRENT_BINARY_DIR}/icu")
    file(MAKE_DIRECTORY "${icu_prefix}/include")
    string(TOUPPER "${CMAKE_BUILD_TYPE}" icu_configuration)
    set(icu_cflags "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${icu_configuration}}")
    set(icu_cxxflags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${icu_configuration}}")
    if(APPLE)
        string(APPEND icu_cflags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
        string(APPEND icu_cxxflags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
        if(CMAKE_OSX_SYSROOT)
            string(APPEND icu_cflags " -isysroot '${CMAKE_OSX_SYSROOT}'")
            string(APPEND icu_cxxflags " -isysroot '${CMAKE_OSX_SYSROOT}'")
        endif()
    endif()
    set(icu_libraries)
    foreach(component IN ITEMS uc i18n data)
        set(library "${icu_prefix}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}icu${component}${CMAKE_SHARED_LIBRARY_SUFFIX}")
        list(APPEND icu_libraries "${library}")
        add_library(ICU::${component} SHARED IMPORTED GLOBAL)
        set_target_properties(ICU::${component} PROPERTIES
            IMPORTED_LOCATION "${library}"
            INTERFACE_INCLUDE_DIRECTORIES "${icu_prefix}/include")
    endforeach()
    set(icu_make_arguments -j4)
    if(PLAYGROUND_PLATFORM STREQUAL "linux")
        # Each shared library must find its siblings after archive relocation.
        # Make consumes one '$'; shell quoting preserves the other for the linker.
        list(APPEND icu_make_arguments "RPATHLDFLAGS=-Wl,-zorigin,-rpath,'$$ORIGIN'")
    endif()
    # ICU ships an Autoconf build. Keep its install entirely inside our build
    # tree; bound nested compilation so Ninja cannot fan out another full build.
    ExternalProject_Add(playground_icu
        SOURCE_DIR "${icu_source_SOURCE_DIR}"
        BINARY_DIR "${icu_source_BINARY_DIR}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
            "CFLAGS=${icu_cflags}" "CXXFLAGS=${icu_cxxflags}"
            "LDFLAGS=${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_${icu_configuration}}"
            "${icu_source_SOURCE_DIR}/source/configure"
            "--prefix=${icu_prefix}" "--libdir=${icu_prefix}/lib"
            --enable-shared --enable-rpath --disable-static --disable-tests --disable-samples
            --disable-extras --with-data-packaging=library
        COMMAND "${icu_make}" clean
        BUILD_COMMAND "${icu_make}" ${icu_make_arguments}
        INSTALL_COMMAND "${icu_make}" ${icu_make_arguments} install
        INSTALL_BYPRODUCTS ${icu_libraries}
        LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON
        LOG_OUTPUT_ON_FAILURE ON)
    foreach(component IN ITEMS uc i18n data)
        add_dependencies(ICU::${component} playground_icu)
    endforeach()
    set(icu_license "${icu_source_SOURCE_DIR}/LICENSE")
endif()
set_property(TARGET ICU::i18n PROPERTY INTERFACE_LINK_LIBRARIES ICU::uc)
set_property(TARGET ICU::uc PROPERTY INTERFACE_LINK_LIBRARIES ICU::data)
install(FILES "${icu_license}"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME icu.txt)
