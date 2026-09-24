#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <platform/sdl/GPUResources.hpp>
#include <rendering/AllocationBudget.hpp>
#include <rendering/RenderFailure.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl::gpu_detail {

template <typename T, auto Release> using Resource = GPUResource<T, Release>;
using Shader = Resource<SDL_GPUShader, SDL_ReleaseGPUShader>;
using Pipeline =
    Resource<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;
using Sampler = Resource<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using Buffer = Resource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using Transfer = Resource<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
using Texture = GPUTextureResource;
using Fence = Resource<SDL_GPUFence, SDL_ReleaseGPUFence>;

// Native attachment only: depth has no RGB encoding/alpha and is not
// PaintImage.
Texture createDepthTarget(GPUDeviceHandle device, math::Vec2i size);

// Reuses a transfer allocation. SDL cycling protects previously encoded
// uploads.
class UploadStream {
  GPUDeviceHandle _device;
  Transfer _transfer;
  Uint32 _capacity{};

public:
  explicit UploadStream(GPUDeviceHandle device) : _device{std::move(device)} {}
  void *map(std::size_t bytes);
  void unmap() noexcept {
    SDL_UnmapGPUTransferBuffer(_device->get(), _transfer.get());
  }
  SDL_GPUTransferBuffer *get() const noexcept { return _transfer.get(); }
  Uint32 capacity() const noexcept { return _capacity; }
};

class StreamBuffer {
  GPUDeviceHandle _device;
  UploadStream _upload;
  Buffer _buffer;
  SDL_GPUBufferUsageFlags _usage;
  Uint32 _capacity{};

public:
  StreamBuffer(GPUDeviceHandle device, SDL_GPUBufferUsageFlags usage)
      : _device{device}, _upload{std::move(device)}, _usage{usage} {}
  SDL_GPUBuffer *write(SDL_GPUCommandBuffer *commands,
                       std::span<const std::byte> bytes);
  Uint32 capacity() const noexcept { return _capacity; }
};

struct DepthTarget {
  math::Vec2i size;
  Texture texture;
  rendering::ResourceLease use;
  bool isLeased() const noexcept { return use.use_count() > 1; }
  rendering::SubmissionId lastSubmission() const noexcept {
    return use->lastSubmission;
  }
};

struct TargetPoolStats {
  std::size_t retainedBytes{};
  std::uint64_t allocations{};
  std::uint64_t reuses{};
  std::uint64_t busyMisses{};
  std::uint64_t evictions{};
  std::uint64_t pressureFailures{};
};

// Reuse requires both exclusive CPU ownership and no recording/submission
// leases. SDL fences retire those leases without a device-wide idle wait.
class TargetPool {
  struct ColorEntry {
    std::shared_ptr<GPUImage> target;
    std::size_t bytes;
    rendering::SubmissionId lastUse{};
  };
  struct DepthEntry {
    std::shared_ptr<DepthTarget> target;
    std::size_t bytes;
    rendering::SubmissionId lastUse{};
  };
  GPUDeviceHandle _device;
  std::vector<ColorEntry> _colors;
  std::vector<DepthEntry> _depths;
  TargetPoolStats _stats;
  rendering::AllocationBudget _allocations;

  bool makeRoom(std::size_t bytes);
  std::shared_ptr<void> reserve(std::size_t bytes);

public:
  explicit TargetPool(GPUDeviceHandle device)
      : _device{std::move(device)},
        _allocations{_device->limits().maxLiveTargetBytes} {}
  std::shared_ptr<GPUImage> color(math::Vec2i size);
  std::shared_ptr<DepthTarget> depth(math::Vec2i size);
  const TargetPoolStats &stats() const noexcept { return _stats; }
  std::size_t liveBytes() const noexcept { return _allocations.bytes(); }
  void trimAged();
  void trim();
};

struct Commands {
  GPUDeviceHandle device;
  SDL_GPUCommandBuffer *value;
  explicit Commands(GPUDeviceHandle owner, std::string_view label = "commands")
      : device{std::move(owner)}, value{device->acquireCommands(label)} {}
  ~Commands() {
    if (value)
      device->cancel(value);
  }
  Commands(const Commands &) = delete;
  Commands &operator=(const Commands &) = delete;
  void submit() {
    auto *commands = std::exchange(value, nullptr);
    try {
      device->submit(commands);
    } catch (...) {
      device->cancel(commands);
      throw;
    }
  }
};

Shader loadShader(GPUDeviceHandle device, const char *name,
                  SDL_GPUShaderStage stage, Uint32 samplers, Uint32 uniforms,
                  Uint32 storageBuffers = 0);

inline SDL_GPUColorTargetBlendState blend() {
  SDL_GPUColorTargetBlendState result{};
  result.enable_blend = true;
  result.src_color_blendfactor = result.src_alpha_blendfactor =
      SDL_GPU_BLENDFACTOR_ONE;
  result.dst_color_blendfactor = result.dst_alpha_blendfactor =
      SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  result.color_blend_op = result.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  return result;
}

inline Sampler sampler(GPUDeviceHandle device) {
  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = info.mag_filter = SDL_GPU_FILTER_NEAREST;
  info.address_mode_u = info.address_mode_v = info.address_mode_w =
      SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  return {device, SDL_CreateGPUSampler(device->get(), &info)};
}

} // namespace playground::sdl::gpu_detail
