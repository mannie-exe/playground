# Resolve a native desktop target once. These identifiers describe the compiler
# output; changing a label must never pretend to select a different toolchain.
if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR "Use a single-configuration generator such as Ninja (see CMakePresets.json)")
endif()
if(CMAKE_CROSSCOMPILING OR NOT CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME)
    message(FATAL_ERROR "Cross-compilation is not supported; configure on the target OS and CPU")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLAYGROUND_PLATFORM windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLAYGROUND_PLATFORM macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLAYGROUND_PLATFORM linux)
else()
    message(FATAL_ERROR "Unsupported desktop OS: ${CMAKE_SYSTEM_NAME}")
endif()

function(playground_normalize_arch input output)
    string(TOLOWER "${input}" arch)
    if(arch MATCHES "^(amd64|x86_64|x64)$")
        set(${output} amd64 PARENT_SCOPE)
    elseif(arch MATCHES "^(arm64|aarch64)$")
        set(${output} arm64 PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unsupported CPU architecture: ${input}; expected AMD64 or Arm64")
    endif()
endfunction()

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES arch_count)
    if(NOT arch_count EQUAL 1)
        message(FATAL_ERROR "Universal builds are not supported; select the host architecture only")
    endif()
endif()
playground_normalize_arch("${CMAKE_HOST_SYSTEM_PROCESSOR}" playground_host_arch)
include(CheckCXXSourceCompiles)
# Architecture flags can change on reconfigure without changing the compiler.
unset(playground_compiler_amd64 CACHE)
unset(playground_compiler_arm64 CACHE)
check_cxx_source_compiles("
#if (!defined(__x86_64__) && !defined(_M_X64)) || defined(_M_ARM64EC)
#error Not AMD64
#endif
int main() { return 0; }" playground_compiler_amd64)
check_cxx_source_compiles("
#if (!defined(__aarch64__) && !defined(_M_ARM64)) || defined(_M_ARM64EC)
#error Not native Arm64
#endif
int main() { return 0; }" playground_compiler_arm64)
if(playground_compiler_amd64)
    set(PLAYGROUND_ARCHITECTURE amd64)
elseif(playground_compiler_arm64)
    set(PLAYGROUND_ARCHITECTURE arm64)
else()
    message(FATAL_ERROR "The compiler must target native AMD64 or Arm64")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
        NOT PLAYGROUND_ARCHITECTURE STREQUAL playground_host_arch)
    message(FATAL_ERROR "Compiler target ${PLAYGROUND_ARCHITECTURE} does not match host ${playground_host_arch}")
endif()
set(PLAYGROUND_TARGET "${PLAYGROUND_PLATFORM}-${PLAYGROUND_ARCHITECTURE}")
message(STATUS "playground native target: ${PLAYGROUND_TARGET}")

include(GNUInstallDirs)
# Keep install directories relative so --prefix and CPack can relocate them.
foreach(directory CMAKE_INSTALL_BINDIR CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_DATADIR)
    if(IS_ABSOLUTE "${${directory}}" OR "${${directory}}" MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "${directory} must be relative and stay inside the portable install")
    endif()
endforeach()
file(RELATIVE_PATH playground_bindir_to_libdir
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/${playground_bindir_to_libdir}")
elseif(PLAYGROUND_PLATFORM STREQUAL "linux")
    set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/${playground_bindir_to_libdir}")
endif()
