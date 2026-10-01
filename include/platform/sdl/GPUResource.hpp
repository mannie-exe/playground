#pragma once

#include <stdexcept>
#include <utility>

#include <platform/sdl/GPUDevice.hpp>
#include <rendering/ResourceEstimate.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {

// Device-dependent native ownership. Release requests deferred native
// retirement; it does not establish GPU completion or make the allocation
// reusable by a pool.
template <typename T, auto Release> class GPUResource {
  GPUDeviceHandle _device;
  T *_value{};
  rendering::ResourceLease _use;

public:
  GPUResource() = default;

  GPUResource(GPUDeviceHandle device, T *value,
              rendering::ResourceLease use = {})
      : _device{std::move(device)}, _value{value}, _use{std::move(use)} {
    if (!_device)
      throw std::invalid_argument("GPU resource requires its creating device");
    if (!_value)
      throwSDLError("GPU resource creation failed");
  }

  ~GPUResource() {
    if (_use && _use->allocation)
      _use->allocation->setState(rendering::AllocationState::Retiring);
    if (_value)
      Release(_device->get(), _value);
  }

  GPUResource(const GPUResource &) = delete;
  GPUResource &operator=(const GPUResource &) = delete;

  GPUResource(GPUResource &&other) noexcept
      : _device{std::move(other._device)},
        _value{std::exchange(other._value, nullptr)},
        _use{std::move(other._use)} {}

  GPUResource &operator=(GPUResource &&other) noexcept {
    if (this != &other) {
      GPUResource temporary{std::move(other)};
      std::swap(_device, temporary._device);
      std::swap(_value, temporary._value);
      std::swap(_use, temporary._use);
    }
    return *this;
  }

  T *get() const noexcept { return _value; }

  const rendering::ResourceLease &use() const noexcept { return _use; }

  explicit operator bool() const noexcept { return _value != nullptr; }
};

using GPUTextureResource = GPUResource<SDL_GPUTexture, SDL_ReleaseGPUTexture>;

template <typename T, auto Create, auto Release, typename Info>
GPUResource<T, Release>
createGPUResource(const GPUDeviceHandle &device, const Info &info,
                  std::size_t bytes, rendering::MemoryClass memory,
                  rendering::ResourceKind kind, std::string_view context,
                  rendering::ResourceLedger::Token reservation = {}) {
  device->checkOwnerThread();
  if (reservation)
    device->resources()->validateReservation(reservation, memory, kind, bytes);
  auto allocation =
      reservation ? std::move(reservation)
                  : device->resources()->reserve(memory, kind, bytes, context);
  auto use = std::make_shared<rendering::ResourceUse>(device->resourceDomain(),
                                                      0, std::move(allocation));
  auto *value = Create(device->get(), &info);
  if (!value)
    throw rendering::ResourceAllocationFailure(
        std::format("{}: native allocation failed after reserving {} bytes: {}",
                    context, bytes, SDL_GetError()));
  GPUResource<T, Release> result{device, value, use};
  use->allocation->setState(rendering::AllocationState::Owned);
  return result;
}

inline std::size_t
estimateTextureStorage(const SDL_GPUTextureCreateInfo &info) {
  const auto blockBytes =
      SDL_CalculateGPUTextureFormatSize(info.format, 1, 1, 1);
  if (!blockBytes || static_cast<unsigned>(info.sample_count) > 3)
    throw std::invalid_argument(
        "Unknown GPU texture storage format/sample count");
  unsigned blockWidth = 1, blockHeight = 1;
  while (blockWidth < 16 &&
         SDL_CalculateGPUTextureFormatSize(info.format, blockWidth + 1, 1, 1) ==
             blockBytes)
    ++blockWidth;
  while (blockHeight < 16 &&
         SDL_CalculateGPUTextureFormatSize(info.format, 1, blockHeight + 1,
                                           1) == blockBytes)
    ++blockHeight;
  return rendering::estimateTextureStorage(
      {info.width, info.height, info.layer_count_or_depth, info.num_levels,
       1u << static_cast<unsigned>(info.sample_count), blockWidth, blockHeight,
       blockBytes, info.type == SDL_GPU_TEXTURETYPE_3D});
}

inline GPUTextureResource
createTexture(const GPUDeviceHandle &device,
              const SDL_GPUTextureCreateInfo &info,
              rendering::ResourceKind kind,
              rendering::ResourceLedger::Token reservation = {}) {
  auto result = createGPUResource<SDL_GPUTexture, SDL_CreateGPUTexture,
                                  SDL_ReleaseGPUTexture>(
      device, info, estimateTextureStorage(info), rendering::MemoryClass::GPU,
      kind, "GPU texture", std::move(reservation));
  device->registerTexture(result.get(), result.use());
  return result;
}

inline auto createBuffer(const GPUDeviceHandle &device,
                         const SDL_GPUBufferCreateInfo &info,
                         rendering::ResourceKind kind) {
  return createGPUResource<SDL_GPUBuffer, SDL_CreateGPUBuffer,
                           SDL_ReleaseGPUBuffer>(
      device, info, info.size, rendering::MemoryClass::GPU, kind, "GPU buffer");
}

inline auto createTransfer(const GPUDeviceHandle &device,
                           const SDL_GPUTransferBufferCreateInfo &info,
                           rendering::ResourceLedger::Token reservation = {}) {
  return createGPUResource<SDL_GPUTransferBuffer, SDL_CreateGPUTransferBuffer,
                           SDL_ReleaseGPUTransferBuffer>(
      device, info, info.size, rendering::MemoryClass::CPU,
      rendering::ResourceKind::Upload, "GPU upload staging",
      std::move(reservation));
}

} // namespace playground::sdl
