/* Playground's versioned SDL Vulkan extension, not an upstream SDL API.
 * Acquired through SDL_GetGPUDeviceProperties; absent means unsupported.
 * No exported symbol or SDL dynapi/driver-vtable ABI is added.
 */
#ifndef SDL_gpu_timestamps_playground_h_
#define SDL_gpu_timestamps_playground_h_

#include <SDL3/SDL_gpu.h>

#define SDL_PROP_GPU_DEVICE_PLAYGROUND_TIMESTAMPS_POINTER "playground.gpu.timestamps.v1"
#define SDL_PLAYGROUND_GPU_TIMESTAMPS_VERSION             1

typedef enum SDL_GPUTimestampReadStatus
{
    SDL_GPU_TIMESTAMP_NOT_READY = 0,
    SDL_GPU_TIMESTAMP_READY = 1,
    SDL_GPU_TIMESTAMP_ERROR = 2,
    SDL_GPU_TIMESTAMP_DEVICE_LOST = 3
} SDL_GPUTimestampReadStatus;

typedef struct SDL_GPUTimestampInterface
{
    Uint32 version;
    Uint32 struct_size;
    Uint32 valid_bits;
    float period_nanoseconds;
    void *context;
    /* One fixed pool per device, 1..4096 paired timestamp slots. */
    void *(SDLCALL *CreatePool)(void *context, Uint32 slots);
    /* Records outside passes. Begin resets the pair, then writes TOP_OF_PIPE.
     * End writes BOTTOM_OF_PIPE. Caller owns cancellation/completion tracking.
     * Pass/submission validation follows SDL's GPU debug mode; the caller must
     * supply a live recording command buffer even when validation is disabled. */
    bool(SDLCALL *Write)(void *pool, SDL_GPUCommandBuffer *command_buffer,
                         Uint32 slot, bool end);
    SDL_GPUTimestampReadStatus(SDLCALL *Read)(void *pool, Uint32 slot,
                                              Uint64 *begin, Uint64 *end);
    /* Closes client access. Native pool remains owned by SDL until device
     * destruction, so releasing after an ambiguous failure cannot free live GPU data. */
    void(SDLCALL *ReleasePool)(void *pool);
} SDL_GPUTimestampInterface;

#endif
