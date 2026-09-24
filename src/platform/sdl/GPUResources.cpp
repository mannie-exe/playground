#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>

#include "GPUInternal.hpp"
#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <rendering/Submission.hpp>
#include <support/SDLError.hpp>
#include <support/SDLResource.hpp>

namespace playground::sdl {

rendering::RGBA8Image packSurfaceRGBA8(const SurfacePaintImage &source) {
  const auto &surface = source.surface();
  rendering::RGBA8Image result{.size = {surface->w, surface->h},
                               .alpha = source.alphaMode(),
                               .encoding = source.colorEncoding()};
  const auto bytes = rendering::RGBA8Image::byteSize(result.size);
  if (bytes > std::numeric_limits<Uint32>::max())
    throw std::length_error("Image exceeds SDL GPU transfer-buffer size");
  SDLResource<SDL_Surface, SDL_DestroySurface> converted{
      SDL_ConvertSurface(surface.get(), SDL_PIXELFORMAT_RGBA32)};
  if (!converted)
    throwSDLError("Failed to convert image for GPU upload");
  result.pixels.resize(bytes);
  const auto rowBytes = static_cast<std::size_t>(surface->w) * 4;
  for (int y = 0; y < surface->h; ++y)
    std::memcpy(result.pixels.data() + static_cast<std::size_t>(y) * rowBytes,
                static_cast<const std::uint8_t *>(converted->pixels) +
                    static_cast<std::size_t>(y) * converted->pitch,
                rowBytes);
  return result;
}

GPUImage::GPUImage(GPUDeviceHandle device, const rendering::RGBA8Image &pixels)
    : _device{std::move(device)}, _size{pixels.size}, _alpha{pixels.alpha},
      _encoding{pixels.encoding} {
  if (!_device)
    throw std::invalid_argument("GPU image requires a device");
  _device->checkOwnerThread();
  _use = std::make_shared<rendering::ResourceUse>(_device->resourceDomain());
  pixels.validate();
  _device->limits().validateTarget(_size, 4);
  _device->limits().validateUpload(pixels.pixels.size());
  auto *rawDevice = _device->get();
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = static_cast<Uint32>(_size.x);
  info.height = static_cast<Uint32>(_size.y);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  if (!SDL_GPUTextureSupportsFormat(rawDevice, info.format, info.type,
                                    info.usage))
    throw std::runtime_error("GPU does not support RGBA8 sampled images");
  GPUTextureResource texture{_device, SDL_CreateGPUTexture(rawDevice, &info)};

  const SDL_GPUTransferBufferCreateInfo transferInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(pixels.pixels.size())};
  gpu_detail::Transfer transfer{
      _device, SDL_CreateGPUTransferBuffer(rawDevice, &transferInfo)};
  void *mapped = SDL_MapGPUTransferBuffer(rawDevice, transfer.get(), false);
  if (!mapped)
    throwSDLError("Failed to map image transfer buffer");
  std::memcpy(mapped, pixels.pixels.data(), pixels.pixels.size());
  SDL_UnmapGPUTransferBuffer(rawDevice, transfer.get());
  gpu_detail::Commands commands{_device, "image upload"};
  _device->recordUse(commands.value, _use);
  auto *copy = SDL_BeginGPUCopyPass(commands.value);
  if (!copy)
    throwSDLError("Failed to begin image upload pass");
  const SDL_GPUTextureTransferInfo source{.transfer_buffer = transfer.get(),
                                          .offset = 0,
                                          .pixels_per_row = info.width,
                                          .rows_per_layer = info.height};
  const SDL_GPUTextureRegion target{
      .texture = texture.get(), .w = info.width, .h = info.height, .d = 1};
  SDL_UploadToGPUTexture(copy, &source, &target, false);
  SDL_EndGPUCopyPass(copy);
  commands.submit();
  _texture = std::move(texture);
  _device->registerTexture(_texture.get(), _use);
}

GPUImage::GPUImage(GPUDeviceHandle device, math::Vec2i size)
    : _device{std::move(device)}, _size{size},
      _alpha{rendering::AlphaMode::Premultiplied},
      _encoding{rendering::ColorEncoding::Linear}, _bytesPerPixel{8} {
  if (!_device)
    throw std::invalid_argument("GPU color target requires a device");
  _device->checkOwnerThread();
  _use = std::make_shared<rendering::ResourceUse>(_device->resourceDomain());
  _device->limits().validateTarget(size, _bytesPerPixel);
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  info.width = Uint32(size.x);
  info.height = Uint32(size.y);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  if (!SDL_GPUTextureSupportsFormat(_device->get(), info.format, info.type,
                                    info.usage))
    throw std::runtime_error(
        "GPU does not support linear sampled color targets");
  _texture = {_device, SDL_CreateGPUTexture(_device->get(), &info)};
  _device->registerTexture(_texture.get(), _use);
}

GPUImagePreparer::GPUImagePreparer(GPUDeviceHandle device)
    : _device{std::move(device)} {
  if (!_device)
    throw std::invalid_argument("GPU image preparer requires a device");
}

void GPUImagePreparer::prune() {
  _device->checkOwnerThread();
  std::erase_if(_cache, [&](const auto &entry) {
    if (!entry.first.expired())
      return false;
    for (const auto &representation : entry.second.representations)
      _residentBytes -= representation.bytes;
    return true;
  });
  trim();
}

void GPUImagePreparer::trim() {
  while (_residentBytes > _device->limits().maxResidentBytes) {
    Representation *oldest{};
    for (auto &[source, entry] : _cache)
      for (auto &candidate : entry.representations)
        if (candidate.image && (!oldest || candidate.lastUse < oldest->lastUse))
          oldest = &candidate;
    if (!oldest)
      break;
    _residentBytes -= oldest->bytes;
    *oldest = {};
  }
  std::erase_if(_cache, [](const auto &entry) {
    return std::ranges::all_of(entry.second.representations,
                               [](const auto &value) { return !value.image; });
  });
}

rendering::PaintImageHandle
GPUImagePreparer::prepare(rendering::PaintImageHandle source) {
  _device->checkOwnerThread();
  if (!source)
    throw std::invalid_argument("GPU image preparation requires a source");
  if (auto gpu = std::dynamic_pointer_cast<const GPUImage>(source)) {
    if (gpu->device() != _device)
      throw std::invalid_argument("GPU image belongs to another device");
    return source;
  }
  auto surface = std::dynamic_pointer_cast<const SurfacePaintImage>(source);
  if (!surface)
    throw std::invalid_argument("Unsupported GPU image source");
  prune();
  // Wraparound must not invert LRU ordering. Clearing residency leaves external
  // immutable handles alive and lets the next request restart the ordering.
  if (_clock == std::numeric_limits<std::uint64_t>::max()) {
    _cache.clear();
    _residentBytes = 0;
    _clock = 0;
  }
  auto &entry = _cache[surface->surface()];
  const std::size_t representation =
      (surface->isPremultiplied() ? 1 : 0) +
      (surface->colorEncoding() == rendering::ColorEncoding::Linear ? 2 : 0);
  auto &slot = entry.representations[representation];
  slot.lastUse = ++_clock;
  if (slot.image)
    return slot.image;
  const auto bytes = _device->limits().validateTarget(
      {surface->surface()->w, surface->surface()->h}, 4);
  _device->limits().validateUpload(bytes);
  auto result = std::make_shared<GPUImage>(_device, packSurfaceRGBA8(*surface));
  if (bytes <= _device->limits().maxResidentBytes) {
    // Trim before addition to keep accounting overflow-free even with huge
    // caps.
    while (_residentBytes > _device->limits().maxResidentBytes - bytes) {
      Representation *oldest{};
      for (auto &[key, value] : _cache)
        for (auto &candidate : value.representations)
          if (candidate.image &&
              (!oldest || candidate.lastUse < oldest->lastUse))
            oldest = &candidate;
      if (!oldest)
        break;
      _residentBytes -= oldest->bytes;
      *oldest = {};
    }
    slot.image = result;
    slot.bytes = bytes;
    _residentBytes += bytes;
  }
  return result;
}

} // namespace playground::sdl
