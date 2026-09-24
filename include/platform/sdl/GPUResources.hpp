#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

#include <SDL3/SDL_gpu.h>

#include <platform/sdl/GPUResource.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <rendering/ImageData.hpp>
#include <rendering/ImagePreparer.hpp>
#include <support/SDLResource.hpp>

namespace playground::sdl {

class GPUImage final : public rendering::PaintImage {
  GPUDeviceHandle _device;
  GPUTextureResource _texture;
  math::Vec2i _size;
  rendering::AlphaMode _alpha;
  rendering::ColorEncoding _encoding;
  std::size_t _bytesPerPixel{4};
  rendering::ResourceLease _use;

public:
  GPUImage(GPUDeviceHandle device, const rendering::RGBA8Image &pixels);
  // Backend target under construction: initialize before publishing as a
  // PaintImageHandle, then never modify while such a handle can be observed.
  GPUImage(GPUDeviceHandle device, math::Vec2i size);
  ~GPUImage() override = default;
  GPUImage(const GPUImage &) = delete;
  GPUImage &operator=(const GPUImage &) = delete;

  math::Size2 pixelSize() const noexcept override {
    return {static_cast<float>(_size.x), static_cast<float>(_size.y)};
  }
  SDL_GPUTexture *get() const noexcept { return _texture.get(); }
  std::size_t bytesPerPixel() const noexcept override { return _bytesPerPixel; }
  const GPUDeviceHandle &device() const noexcept { return _device; }
  const rendering::ResourceLease &use() const noexcept { return _use; }
  bool isLeased() const noexcept { return _use.use_count() > 1; }
  rendering::SubmissionId lastSubmission() const noexcept {
    return _use->lastSubmission;
  }
  rendering::AlphaMode alphaMode() const noexcept override { return _alpha; }
  rendering::ColorEncoding colorEncoding() const noexcept override {
    return _encoding;
  }
};

rendering::RGBA8Image packSurfaceRGBA8(const SurfacePaintImage &source);

class GPUImagePreparer : public rendering::ImagePreparer {
  struct Representation {
    rendering::PaintImageHandle image;
    std::size_t bytes{};
    std::uint64_t lastUse{};
  };
  struct Entry {
    std::array<Representation, 4> representations;
  };

  GPUDeviceHandle _device;
  std::size_t _residentBytes{};
  std::uint64_t _clock{};

  std::map<std::weak_ptr<SDL_Surface>, Entry,
           std::owner_less<std::weak_ptr<SDL_Surface>>>
      _cache;

  void trim();

public:
  explicit GPUImagePreparer(GPUDeviceHandle device);
  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return _device->resourceDomain();
  }
  rendering::PaintImageHandle
  prepare(rendering::PaintImageHandle source) override;
  void prune();
  std::size_t residentBytes() const noexcept { return _residentBytes; }
};

} // namespace playground::sdl
