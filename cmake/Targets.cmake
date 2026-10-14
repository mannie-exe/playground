# Shared target setup; applications register their own installable executables.
function(playground_register_install_target target_name)
    set_property(GLOBAL APPEND PROPERTY playground_install_targets ${target_name})
endfunction()

function(playground_configure_cpp_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_compile_options(${target_name} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_include_directories(${target_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include)
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
