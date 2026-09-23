#include <platform/sdl/GPUResources.hpp>

#include <cstring>
#include <limits>
#include <support/SDLError.hpp>
#include <utility>

namespace playground::sdl {
namespace {

template <class T, auto Release> struct DeviceResource {
  SDL_GPUDevice *device;
  T *value{};
  ~DeviceResource() {
    if (value)
      Release(device, value);
  }
  DeviceResource(SDL_GPUDevice *device, T *value)
      : device{device}, value{value} {}
  DeviceResource(const DeviceResource &) = delete;
  DeviceResource &operator=(const DeviceResource &) = delete;
};

struct Commands {
  SDL_GPUCommandBuffer *value;
  ~Commands() {
    if (value)
      SDL_CancelGPUCommandBuffer(value);
  }
};

} // namespace

GPUDevice::GPUDevice(GPUDeviceProps props) {
  if (!props.shaderFormats)
    throw std::invalid_argument("GPU device requires supported shader formats");
  _device.reset(
      SDL_CreateGPUDevice(props.shaderFormats, props.debug, props.driver));
  if (!_device)
    throwSDLError("Failed to create GPU device");
}

rendering::RGBA8Image packSurfaceRGBA8(const SurfacePaintImage &source) {
  const auto &surface = source.surface();
  rendering::RGBA8Image result{
      .size = {surface->w, surface->h},
      .alpha = source.isPremultiplied() ? rendering::AlphaMode::Premultiplied
                                        : rendering::AlphaMode::Straight};
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
    : _device{std::move(device)}, _texture{nullptr}, _size{pixels.size},
      _alpha{pixels.alpha} {
  if (!_device)
    throw std::invalid_argument("GPU image requires a device");
  pixels.validate();
  if (pixels.pixels.size() > std::numeric_limits<Uint32>::max())
    throw std::length_error("Image exceeds SDL GPU transfer-buffer size");
  auto *rawDevice = _device->get();
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = static_cast<Uint32>(_size.x);
  info.height = static_cast<Uint32>(_size.y);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  DeviceResource<SDL_GPUTexture, SDL_ReleaseGPUTexture> texture{
      rawDevice, SDL_CreateGPUTexture(rawDevice, &info)};
  if (!texture.value)
    throwSDLError("Failed to create GPU image texture");

  const SDL_GPUTransferBufferCreateInfo transferInfo{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(pixels.pixels.size())};
  DeviceResource<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer> transfer{
      rawDevice, SDL_CreateGPUTransferBuffer(rawDevice, &transferInfo)};
  if (!transfer.value)
    throwSDLError("Failed to create image transfer buffer");
  void *mapped = SDL_MapGPUTransferBuffer(rawDevice, transfer.value, false);
  if (!mapped)
    throwSDLError("Failed to map image transfer buffer");
  std::memcpy(mapped, pixels.pixels.data(), pixels.pixels.size());
  SDL_UnmapGPUTransferBuffer(rawDevice, transfer.value);
  Commands commands{SDL_AcquireGPUCommandBuffer(rawDevice)};
  if (!commands.value)
    throwSDLError("Failed to acquire upload commands");
  auto *copy = SDL_BeginGPUCopyPass(commands.value);
  if (!copy)
    throwSDLError("Failed to begin image upload pass");
  const SDL_GPUTextureTransferInfo source{.transfer_buffer = transfer.value,
                                          .offset = 0,
                                          .pixels_per_row = info.width,
                                          .rows_per_layer = info.height};
  const SDL_GPUTextureRegion target{
      .texture = texture.value, .w = info.width, .h = info.height, .d = 1};
  SDL_UploadToGPUTexture(copy, &source, &target, false);
  SDL_EndGPUCopyPass(copy);
  auto *submitted = std::exchange(commands.value, nullptr);
  if (!SDL_SubmitGPUCommandBuffer(submitted))
    throwSDLError("Failed to submit image upload");
  _texture = std::exchange(texture.value, nullptr);
}

GPUImage::~GPUImage() {
  if (_texture)
    SDL_ReleaseGPUTexture(_device->get(), _texture);
}

GPUImagePreparer::GPUImagePreparer(GPUDeviceHandle device)
    : _device{std::move(device)} {
  if (!_device)
    throw std::invalid_argument("GPU image preparer requires a device");
}

void GPUImagePreparer::prune() {
  std::erase_if(_cache, [](const auto &entry) {
    return entry.first.expired() || (entry.second.straight.expired() &&
                                     entry.second.premultiplied.expired());
  });
}

ui::PaintImageHandle GPUImagePreparer::prepare(ui::PaintImageHandle source) {
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
  auto &entry = _cache[surface->surface()];
  auto &slot =
      surface->isPremultiplied() ? entry.premultiplied : entry.straight;
  if (auto cached = slot.lock())
    return cached;
  auto result = std::make_shared<GPUImage>(_device, packSurfaceRGBA8(*surface));
  slot = result;
  return result;
}

} // namespace playground::sdl
