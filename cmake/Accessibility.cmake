# Official, checksum-pinned AccessKit C release includes desktop native adapters.
CPMAddPackage(NAME accesskit_binary
    URL https://github.com/AccessKit/accesskit-c/releases/download/0.23.1/accesskit-c-0.23.1.zip
    URL_HASH SHA256=35b7ca8a6f1e038b5da35e1e9e5a0adaed9bfcf21e1496d29598fbbadcc7043f
    DOWNLOAD_ONLY YES)
find_package(accesskit CONFIG REQUIRED PATHS ${accesskit_binary_SOURCE_DIR} NO_DEFAULT_PATH)

include(${CMAKE_CURRENT_LIST_DIR}/ICU.cmake)

install(FILES ${accesskit_binary_SOURCE_DIR}/LICENSE-MIT ${accesskit_binary_SOURCE_DIR}/LICENSE-APACHE ${accesskit_binary_SOURCE_DIR}/LICENSE.chromium
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground/licenses/accesskit)
