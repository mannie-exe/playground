#pragma once

#include <stdexcept>
#include <utility>

#include <platform/sdl/GPUDevice.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {

// Device-dependent native ownership. Release requests deferred native
// retirement; it does not establish GPU completion or make the allocation
// reusable by a pool.
template <typename T, auto Release> class GPUResource {
  GPUDeviceHandle _device;
  T *_value{};

public:
  GPUResource() = default;

  GPUResource(GPUDeviceHandle device, T *value)
      : _device{std::move(device)}, _value{value} {
    if (!_device)
      throw std::invalid_argument("GPU resource requires its creating device");
    if (!_value)
      throwSDLError("GPU resource creation failed");
  }

  ~GPUResource() {
    if (_value)
      Release(_device->get(), _value);
  }

  GPUResource(const GPUResource &) = delete;
  GPUResource &operator=(const GPUResource &) = delete;

  GPUResource(GPUResource &&other) noexcept
      : _device{std::move(other._device)},
        _value{std::exchange(other._value, nullptr)} {}

  GPUResource &operator=(GPUResource &&other) noexcept {
    if (this != &other) {
      GPUResource temporary{std::move(other)};
      std::swap(_device, temporary._device);
      std::swap(_value, temporary._value);
    }
    return *this;
  }

  T *get() const noexcept { return _value; }

  explicit operator bool() const noexcept { return _value != nullptr; }
};

using GPUTextureResource = GPUResource<SDL_GPUTexture, SDL_ReleaseGPUTexture>;

} // namespace playground::sdl
