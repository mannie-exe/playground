# Reproducible, pinned Vulkan-only SDL extension. The checked originals guard
# against quietly applying substitutions to an unrelated SDL version or edits.
function(playground_sdl_original file expected_hash output)
    set(original "${file}.playground-original")
    if(NOT EXISTS "${original}")
        file(SHA256 "${file}" actual_hash)
        if(NOT actual_hash STREQUAL expected_hash)
            message(FATAL_ERROR "SDL timestamp patch source mismatch: ${file}")
        endif()
        file(COPY_FILE "${file}" "${original}")
    endif()
    file(SHA256 "${original}" original_hash)
    if(NOT original_hash STREQUAL expected_hash)
        message(FATAL_ERROR "SDL timestamp patch original mismatch: ${original}")
    endif()
    file(READ "${original}" body)
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

function(playground_sdl_commit file original replacement)
    file(READ "${file}" current)
    if(NOT current STREQUAL original AND NOT current STREQUAL replacement)
        message(FATAL_ERROR "SDL timestamp patch would overwrite unknown edits: ${file}")
    endif()
    if(NOT current STREQUAL replacement)
        file(WRITE "${file}" "${replacement}")
    endif()
endfunction()

function(playground_apply_sdl_timestamps source_dir)
    set(patch_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/sdl")
    set(vulkan_dir "${source_dir}/src/gpu/vulkan")
    set(source "${vulkan_dir}/SDL_gpu_vulkan.c")
    playground_sdl_original("${source}"
        "35903d00b75a6ec9b119f576bd1ed040dfac997b4184af415d9b4e23afa3af33" original)
    set(body "${original}")
    string(REPLACE "#include <SDL3/SDL_vulkan.h>"
        "#include <SDL3/SDL_vulkan.h>\n#include <SDL3/SDL_gpu_timestamps_playground.h>" body "${body}")
    string(REPLACE "    SDL_PropertiesID props;"
        "    SDL_PropertiesID props;\n    SDL_GPUTimestampInterface playgroundTimestampInterface;\n    void *playgroundTimestampPool;" body "${body}")
    string(REPLACE "static void VULKAN_DestroyDevice("
        "#include \"SDL_gpu_vulkan_timestamps_playground.inc\"\n\nstatic void VULKAN_DestroyDevice(" body "${body}")
    string(REPLACE "    SDL_free(renderer->submittedCommandBuffers);"
        "    PlaygroundVulkan_DestroyTimestamps(renderer);\n    SDL_free(renderer->submittedCommandBuffers);" body "${body}")
    string(REPLACE "    // FIXME: just move this into this function"
        "    PlaygroundVulkan_InitTimestamps(renderer);\n\n    // FIXME: just move this into this function" body "${body}")
    playground_sdl_commit("${source}" "${original}" "${body}")

    set(functions "${vulkan_dir}/SDL_gpu_vulkan_vkfuncs.h")
    playground_sdl_original("${functions}"
        "75921837d29c19f4c3b3b0e66e3c1db97a589cb2e928985c4e844948857e1258" original)
    string(REPLACE "VULKAN_DEVICE_FUNCTION(vkCreateBuffer)"
        "VULKAN_DEVICE_FUNCTION(vkCreateQueryPool)\nVULKAN_DEVICE_FUNCTION(vkDestroyQueryPool)\nVULKAN_DEVICE_FUNCTION(vkCmdResetQueryPool)\nVULKAN_DEVICE_FUNCTION(vkCmdWriteTimestamp)\nVULKAN_DEVICE_FUNCTION(vkGetQueryPoolResults)\nVULKAN_DEVICE_FUNCTION(vkCreateBuffer)" body "${original}")
    playground_sdl_commit("${functions}" "${original}" "${body}")
    configure_file("${patch_dir}/SDL_gpu_timestamps_playground.h"
        "${source_dir}/include/SDL3/SDL_gpu_timestamps_playground.h" COPYONLY)
    configure_file("${patch_dir}/SDL_gpu_vulkan_timestamps_playground.inc"
        "${vulkan_dir}/SDL_gpu_vulkan_timestamps_playground.inc" COPYONLY)
endfunction()
