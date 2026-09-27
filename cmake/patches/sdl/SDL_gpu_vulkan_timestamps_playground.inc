/* Playground timestamp extension for the pinned SDL Vulkan backend.
 * Kept separate from upstream implementation; see SDL_gpu_timestamps_playground.h.
 */
typedef struct PlaygroundVulkanTimestampPool
{
    VulkanRenderer *renderer;
    VkQueryPool queries;
    Uint32 slots;
    bool released;
} PlaygroundVulkanTimestampPool;

static void *SDLCALL PlaygroundVulkan_CreateTimestampPool(void *context, Uint32 slots)
{
    VulkanRenderer *renderer = (VulkanRenderer *)context;
    PlaygroundVulkanTimestampPool *pool;
    VkQueryPoolCreateInfo info;
    VkResult result;
    if (!slots || slots > 4096 || renderer->playgroundTimestampPool) {
        SDL_SetError("Timestamp pool requires 1..4096 slots and one pool per device");
        return NULL;
    }
    pool = SDL_calloc(1, sizeof(*pool));
    if (!pool) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zero(info);
    info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = slots * 2;
    result = renderer->vkCreateQueryPool(renderer->logicalDevice, &info, NULL, &pool->queries);
    if (result != VK_SUCCESS) {
        SDL_SetError("vkCreateQueryPool failed: %d", (int)result);
        SDL_free(pool);
        return NULL;
    }
    pool->renderer = renderer;
    pool->slots = slots;
    renderer->playgroundTimestampPool = pool;
    return pool;
}

static bool SDLCALL PlaygroundVulkan_WriteTimestamp(void *opaque, SDL_GPUCommandBuffer *commandBuffer,
                                                    Uint32 slot, bool end)
{
    PlaygroundVulkanTimestampPool *pool = (PlaygroundVulkanTimestampPool *)opaque;
    VulkanCommandBuffer *commands = (VulkanCommandBuffer *)commandBuffer;
    if (!pool || pool->released || !commands || slot >= pool->slots ||
        commands->renderer != pool->renderer) {
        return SDL_SetError("Timestamp write requires a live same-device command buffer outside passes");
    }
    /* SDL only resets these validation fields when debug mode is enabled.
     * In particular, submitted stays true on recycled non-debug buffers. */
    if (commands->common.device->debug_mode &&
        (commands->common.submitted || commands->common.render_pass.in_progress ||
         commands->common.compute_pass.in_progress || commands->common.copy_pass.in_progress)) {
        return SDL_SetError("Timestamp write requires a live same-device command buffer outside passes");
    }
    if (!end) {
        pool->renderer->vkCmdResetQueryPool(commands->commandBuffer, pool->queries, slot * 2, 2);
    }
    pool->renderer->vkCmdWriteTimestamp(commands->commandBuffer,
                                        end ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        pool->queries, slot * 2 + (end ? 1 : 0));
    return true;
}

static SDL_GPUTimestampReadStatus SDLCALL PlaygroundVulkan_ReadTimestamps(void *opaque, Uint32 slot,
                                                                          Uint64 *begin, Uint64 *end)
{
    PlaygroundVulkanTimestampPool *pool = (PlaygroundVulkanTimestampPool *)opaque;
    Uint64 result[4] = { 0, 0, 0, 0 };
    VkResult status;
    if (!pool || pool->released || slot >= pool->slots || !begin || !end) {
        SDL_SetError("Invalid timestamp result request");
        return SDL_GPU_TIMESTAMP_ERROR;
    }
    status = pool->renderer->vkGetQueryPoolResults(pool->renderer->logicalDevice, pool->queries,
                                                   slot * 2, 2, sizeof(result), result, sizeof(Uint64) * 2,
                                                   VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (status == VK_ERROR_DEVICE_LOST) {
        SDL_SetError("Vulkan timestamp device lost");
        return SDL_GPU_TIMESTAMP_DEVICE_LOST;
    }
    if (status != VK_SUCCESS && status != VK_NOT_READY) {
        SDL_SetError("vkGetQueryPoolResults failed: %d", (int)status);
        return SDL_GPU_TIMESTAMP_ERROR;
    }
    if (status == VK_NOT_READY || !result[1] || !result[3]) {
        return SDL_GPU_TIMESTAMP_NOT_READY;
    }
    *begin = result[0];
    *end = result[2];
    return SDL_GPU_TIMESTAMP_READY;
}

static void SDLCALL PlaygroundVulkan_ReleaseTimestampPool(void *opaque)
{
    PlaygroundVulkanTimestampPool *pool = (PlaygroundVulkanTimestampPool *)opaque;
    if (pool) {
        pool->released = true;
    }
}

static void PlaygroundVulkan_DestroyTimestamps(VulkanRenderer *renderer)
{
    PlaygroundVulkanTimestampPool *pool = (PlaygroundVulkanTimestampPool *)renderer->playgroundTimestampPool;
    if (pool) {
        renderer->vkDestroyQueryPool(renderer->logicalDevice, pool->queries, NULL);
        SDL_free(pool);
        renderer->playgroundTimestampPool = NULL;
    }
}

static void PlaygroundVulkan_InitTimestamps(VulkanRenderer *renderer)
{
    Uint32 count = 0;
    VkQueueFamilyProperties *families;
    SDL_GPUTimestampInterface *api = &renderer->playgroundTimestampInterface;
    renderer->vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &count, NULL);
    families = SDL_calloc(count, sizeof(*families));
    if (!families) {
        return; /* Telemetry is optional; allocation failure does not invalidate rendering. */
    }
    renderer->vkGetPhysicalDeviceQueueFamilyProperties(renderer->physicalDevice, &count, families);
    if (renderer->queueFamilyIndex < count) {
        api->valid_bits = families[renderer->queueFamilyIndex].timestampValidBits;
    }
    SDL_free(families);
    api->period_nanoseconds = renderer->physicalDeviceProperties.properties.limits.timestampPeriod;
    if (!api->valid_bits || api->valid_bits > 64 || api->period_nanoseconds <= 0) {
        return;
    }
    api->version = SDL_PLAYGROUND_GPU_TIMESTAMPS_VERSION;
    api->struct_size = sizeof(*api);
    api->context = renderer;
    api->CreatePool = PlaygroundVulkan_CreateTimestampPool;
    api->Write = PlaygroundVulkan_WriteTimestamp;
    api->Read = PlaygroundVulkan_ReadTimestamps;
    api->ReleasePool = PlaygroundVulkan_ReleaseTimestampPool;
    SDL_SetPointerProperty(renderer->props, SDL_PROP_GPU_DEVICE_PLAYGROUND_TIMESTAMPS_POINTER, api);
}
