#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/RenderError.hpp>
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

// Writable attachments stay internal. Only a successfully submitted result is
// published as an immutable PaintImage; observers prevent pool reuse.
class ColorTarget {
  friend class TargetPool;
  std::shared_ptr<GPUImage> _image;
  rendering::SubmissionId _writeStart{};

  void beginWrite() noexcept { _writeStart = lastSubmission(); }

public:
  ColorTarget(GPUDeviceHandle device, math::Vec2i size,
              rendering::ResourceLedger::Token reservation = {})
      : _image{new GPUImage{std::move(device), size, std::move(reservation)}} {}

  ColorTarget(const ColorTarget &) = delete;
  ColorTarget &operator=(const ColorTarget &) = delete;

  SDL_GPUTexture *get() const noexcept { return _image->get(); }

  math::Size2 pixelSize() const noexcept { return _image->pixelSize(); }

  bool isLeased() const noexcept {
    return _image.use_count() > 1 || _image->isLeased();
  }

  rendering::SubmissionId lastSubmission() const noexcept {
    return _image->lastSubmission();
  }

  std::shared_ptr<const GPUImage> publish() const {
    _image->device()->checkOwnerThread();
    if (lastSubmission() <= _writeStart)
      throw std::logic_error("Cannot publish an unsubmitted color target");
    return _image;
  }
};

// Native attachment only: depth has no RGB encoding/alpha and is not
// PaintImage.
Texture createDepthTarget(GPUDeviceHandle device, math::Vec2i size,
                          rendering::ResourceLedger::Token reservation = {});

// Explicit backing slots: never ask SDL to create hidden cycling allocations.
// A slot is reusable only after all recorded/submitted leases retire.
class UploadStream {
  struct Slot {
    Transfer transfer;
    Uint32 capacity;
  };

  GPUDeviceHandle _device;
  std::vector<Slot> _slots;
  std::size_t _current{};

public:
  explicit UploadStream(GPUDeviceHandle device) : _device{std::move(device)} {}

  void *map(std::size_t bytes);

  void unmap() noexcept { SDL_UnmapGPUTransferBuffer(_device->get(), get()); }

  SDL_GPUTransferBuffer *get() const noexcept {
    return _slots[_current].transfer.get();
  }

  Uint32 capacity() const noexcept {
    return _slots.empty() ? 0 : _slots[_current].capacity;
  }

  void record(SDL_GPUCommandBuffer *commands) {
    _device->recordUse(commands, _slots[_current].transfer.use());
  }

  void trim();
};

class StreamBuffer {
  struct Slot {
    Buffer buffer;
    Uint32 capacity;
  };

  GPUDeviceHandle _device;
  UploadStream _upload;
  std::vector<Slot> _slots;
  SDL_GPUBufferUsageFlags _usage;
  Uint32 _capacity{};

public:
  StreamBuffer(GPUDeviceHandle device, SDL_GPUBufferUsageFlags usage)
      : _device{device}, _upload{std::move(device)}, _usage{usage} {}

  SDL_GPUBuffer *write(SDL_GPUCommandBuffer *commands,
                       std::span<const std::byte> bytes);

  Uint32 capacity() const noexcept { return _capacity; }

  void trim();
};

struct DepthTarget {
  math::Vec2i size;
  Texture texture;

  bool isLeased() const noexcept { return texture.use().use_count() > 1; }

  rendering::SubmissionId lastSubmission() const noexcept {
    return texture.use()->lastSubmission;
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
    std::shared_ptr<ColorTarget> target;
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

  bool makeRoom(std::size_t bytes);
  void makeAllocationRoom(std::size_t bytes);

public:
  explicit TargetPool(GPUDeviceHandle device) : _device{std::move(device)} {}

  std::shared_ptr<ColorTarget>
  color(math::Vec2i size, std::string_view context = "GPU pooled color target",
        rendering::ResourceLedger::Token reservation = {});
  std::shared_ptr<DepthTarget>
  depth(math::Vec2i size, rendering::ResourceLedger::Token reservation = {});

  struct SceneTargets {
    std::shared_ptr<ColorTarget> color, output;
    std::shared_ptr<DepthTarget> depth;
  };

  SceneTargets sceneTargets(math::Vec2i size, bool postProcess);

  const TargetPoolStats &stats() const noexcept { return _stats; }

  std::size_t liveBytes() const {
    return _device->resources()
        ->snapshot()
        .kinds[static_cast<std::size_t>(rendering::ResourceKind::Target)]
        .bytes;
  }

  void trimAged();
  void trim();
};

struct Commands {
  GPUDeviceHandle device;
  SDL_GPUCommandBuffer *value;

  explicit Commands(GPUDeviceHandle owner, std::string_view label = "commands",
                    rendering::GPUWorkContext context = {})
      : device{std::move(owner)},
        value{device->acquireCommands(label, context)} {}

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

inline Sampler sampler(GPUDeviceHandle device,
                       SDL_GPUFilter filter = SDL_GPU_FILTER_NEAREST) {
  SDL_GPUSamplerCreateInfo info{};
  info.min_filter = info.mag_filter = filter;
  info.address_mode_u = info.address_mode_v = info.address_mode_w =
      SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  return {device, SDL_CreateGPUSampler(device->get(), &info)};
}

} // namespace playground::sdl::gpu_detail
