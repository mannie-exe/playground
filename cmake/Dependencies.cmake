# Reuse only the pinned bootstrap, including when configuring without network.
set(playground_cpm_file "${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake")
set(playground_cpm_sha256 1c40fc102ce9625d7de7eb14f541cab30cc3138dca627f0b0ec40293ce6c2934)
if(EXISTS "${playground_cpm_file}")
    file(SHA256 "${playground_cpm_file}" playground_cpm_actual_sha256)
endif()
if(NOT playground_cpm_actual_sha256 STREQUAL playground_cpm_sha256)
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.43.1/CPM.cmake
        "${playground_cpm_file}.download"
        STATUS playground_cpm_status
        TLS_VERIFY ON)
    list(GET playground_cpm_status 0 playground_cpm_status_code)
    list(GET playground_cpm_status 1 playground_cpm_status_message)
    if(NOT playground_cpm_status_code EQUAL 0)
        message(FATAL_ERROR
            "Cannot download CPM.cmake 0.43.1: ${playground_cpm_status_message}. "
            "Configure with network access, or place the checksum-pinned release at "
            "${playground_cpm_file}. Dependency sources must also be available.")
    endif()
    file(SHA256 "${playground_cpm_file}.download" playground_cpm_actual_sha256)
    if(NOT playground_cpm_actual_sha256 STREQUAL playground_cpm_sha256)
        message(FATAL_ERROR "CPM.cmake 0.43.1 checksum mismatch; refusing to include the download")
    endif()
    file(RENAME "${playground_cpm_file}.download" "${playground_cpm_file}")
endif()
include("${playground_cpm_file}")
