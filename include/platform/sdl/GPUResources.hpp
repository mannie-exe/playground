#pragma once

#include <SDL3/SDL_gpu.h>
#include <map>
#include <memory>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <rendering/ImageData.hpp>
#include <rendering/ImagePreparer.hpp>
#include <support/SDLResource.hpp>

namespace playground::sdl {

struct GPUDeviceProps {
  SDL_GPUShaderFormat shaderFormats;
  bool debug{true};
  const char *driver{nullptr};
};

// Opt-in only: no device is created by AppHost yet. SDL initialization outlives
// the device; resources retain it. Use on the renderer thread.
class GPUDevice {
  SDLResource<SDL_GPUDevice, SDL_DestroyGPUDevice> _device;

public:
  explicit GPUDevice(GPUDeviceProps props);
  SDL_GPUDevice *get() const noexcept { return _device.get(); }
  GPUDevice(const GPUDevice &) = delete;
  GPUDevice &operator=(const GPUDevice &) = delete;
};
using GPUDeviceHandle = std::shared_ptr<GPUDevice>;

class GPUImage final : public ui::PaintImage {
  GPUDeviceHandle _device;
  SDL_GPUTexture *_texture;
  math::Vec2i _size;
  rendering::AlphaMode _alpha;

public:
  GPUImage(GPUDeviceHandle device, const rendering::RGBA8Image &pixels);
  ~GPUImage() override;
  GPUImage(const GPUImage &) = delete;
  GPUImage &operator=(const GPUImage &) = delete;

  math::Size2 pixelSize() const noexcept override {
    return {static_cast<float>(_size.x), static_cast<float>(_size.y)};
  }
  SDL_GPUTexture *get() const noexcept { return _texture; }
  const GPUDeviceHandle &device() const noexcept { return _device; }
  rendering::AlphaMode alphaMode() const noexcept { return _alpha; }
};

rendering::RGBA8Image packSurfaceRGBA8(const SurfacePaintImage &source);

class GPUImagePreparer final : public rendering::ImagePreparer {
  struct Entry {
    std::weak_ptr<const ui::PaintImage> straight, premultiplied;
  };

  GPUDeviceHandle _device;

  std::map<std::weak_ptr<SDL_Surface>, Entry,
           std::owner_less<std::weak_ptr<SDL_Surface>>>
      _cache;

public:
  explicit GPUImagePreparer(GPUDeviceHandle device);
  ui::PaintImageHandle prepare(ui::PaintImageHandle source) override;
  void prune();
};

} // namespace playground::sdl
