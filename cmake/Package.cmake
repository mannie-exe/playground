# Archive packaging reuses install rules; it never publishes or selects a target.
set(CPACK_PACKAGE_NAME playground)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
configure_file(${CMAKE_CURRENT_LIST_DIR}/CPackProjectConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CPackProjectConfig.cmake")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_DIRECTORY "${PROJECT_SOURCE_DIR}/dist/packages")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
if(PLAYGROUND_PLATFORM STREQUAL "windows")
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()

set(playground_packaged_renderer "\"software\"")
if(PLAYGROUND_GPU)
    set(playground_packaged_renderer "\"software\", \"vulkan\"")
endif()
configure_file(${CMAKE_CURRENT_LIST_DIR}/BuildInfo.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/build-info.json.in @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/build-info.json"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/build-info.json.in")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>/build-info.json"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/playground)
include(CPack)
