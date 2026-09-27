# Official, checksum-pinned AccessKit C release includes desktop native adapters.
CPMAddPackage(NAME accesskit_binary
    URL https://github.com/AccessKit/accesskit-c/releases/download/0.23.1/accesskit-c-0.23.1.zip
    URL_HASH SHA256=35b7ca8a6f1e038b5da35e1e9e5a0adaed9bfcf21e1496d29598fbbadcc7043f
    DOWNLOAD_ONLY YES)
find_package(accesskit CONFIG REQUIRED PATHS ${accesskit_binary_SOURCE_DIR} NO_DEFAULT_PATH)

# ICU's Windows archive uses the supported MSVC ABI for Debug and Release.
# Other toolchains use their installed ICU development package.
if(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(_icu_arch WinARM64)
        set(_icu_hash 34fedefb5aa7e3a31e77a8c4befd33b0020c3cc8056ec7d73550f2eeb40b3d85)
    else()
        set(_icu_arch Win64)
        set(_icu_hash 446b671f9437227daa79e221d4521d75793f9ecd65ac44c06e34dd848f201ac2)
    endif()
    CPMAddPackage(NAME icu_binary
        URL https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-${_icu_arch}-MSVC2022.zip
        URL_HASH SHA256=${_icu_hash} DOWNLOAD_ONLY YES)
    set(ICU_ROOT ${icu_binary_SOURCE_DIR})
endif()
find_package(ICU 78 REQUIRED COMPONENTS uc i18n data)
if(WIN32 AND icu_binary_SOURCE_DIR)
    file(GLOB _icu_dlls "${icu_binary_SOURCE_DIR}/bin64/icu*.dll" "${icu_binary_SOURCE_DIR}/bin/icu*.dll")
    file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    file(COPY ${_icu_dlls} DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    install(FILES ${_icu_dlls} DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${icu_binary_SOURCE_DIR}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses RENAME icu.txt)
endif()
install(FILES ${accesskit_binary_SOURCE_DIR}/LICENSE-MIT ${accesskit_binary_SOURCE_DIR}/LICENSE-APACHE ${accesskit_binary_SOURCE_DIR}/LICENSE.chromium
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/accesskit)
