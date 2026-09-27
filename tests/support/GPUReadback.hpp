#pragma once
#include <array>
#include <cmath>
#include <vector>

#include "../../src/platform/sdl/GPUInternal.hpp"
#include "Test.hpp"

namespace playground::test {
using sdl::GPUDeviceHandle;
using sdl::GPUImage;
using sdl::gpu_detail::ColorTarget;
using sdl::gpu_detail::Commands;
using sdl::gpu_detail::Transfer;

inline std::vector<std::array<float, 4>> readPixels(GPUDeviceHandle device,
                                                    const GPUImage &image) {
  const auto size = image.pixelSize();
  SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                       Uint32(size.width * size.height * 8)};
  Transfer transfer{device, SDL_CreateGPUTransferBuffer(device->get(), &info)};
  Commands commands{device};
  device->recordTexture(commands.value, image.get());
  auto *copy = SDL_BeginGPUCopyPass(commands.value);
  test::require(copy != nullptr, "readback copy pass");
  const SDL_GPUTextureRegion source{.texture = image.get(),
                                    .w = Uint32(size.width),
                                    .h = Uint32(size.height),
                                    .d = 1};
  const SDL_GPUTextureTransferInfo target{.transfer_buffer = transfer.get(),
                                          .pixels_per_row = Uint32(size.width),
                                          .rows_per_layer =
                                              Uint32(size.height)};
  SDL_DownloadFromGPUTexture(copy, &source, &target);
  SDL_EndGPUCopyPass(copy);
  commands.submit();
  test::require(SDL_WaitForGPUIdle(device->get()), "readback completion");
  device->pollCompletions();
  auto *bytes = static_cast<Uint16 *>(
      SDL_MapGPUTransferBuffer(device->get(), transfer.get(), false));
  test::require(bytes != nullptr, "readback mapping");
  const auto half = [](Uint16 bits) {
    const int e = (bits >> 10) & 31;
    const float m = (bits & 1023) / 1024.f;
    return (bits & 32768 ? -1.f : 1.f) *
           (e ? std::ldexp(1 + m, e - 15) : std::ldexp(m, -14));
  };
  std::vector<std::array<float, 4>> result(
      std::size_t(size.width * size.height));
  for (std::size_t pixel = 0; pixel < result.size(); ++pixel)
    for (int c = 0; c < 4; ++c)
      result[pixel][c] = half(bytes[pixel * 4 + c]);
  SDL_UnmapGPUTransferBuffer(device->get(), transfer.get());
  return result;
}

inline std::array<float, 4> readPixel(GPUDeviceHandle device,
                                      const GPUImage &image, int x, int y) {
  return readPixels(device,
                    image)[std::size_t(y * int(image.pixelSize().width) + x)];
}

inline std::array<float, 4> readPixel(GPUDeviceHandle device,
                                      const ColorTarget &target, int x, int y) {
  return readPixel(std::move(device), *target.publish(), x, y);
}

} // namespace playground::test
