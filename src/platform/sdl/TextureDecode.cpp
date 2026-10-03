#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#include <stb_image.h>

#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/TextureDecode.hpp>

namespace playground::sdl {
PreparationBudget &texturePreparationBudget() {
  return resourcePreparationBudget();
}

rendering::TextureHandle
decodeTexture(std::span<const std::byte> bytes, std::string_view mime,
              rendering::TextureRole role, rendering::MipPolicy mip,
              std::size_t maximumBytes, PreparationBudget &budget) {
  if (mime == "image/ktx2" ||
      (bytes.size() >= 12 &&
       std::memcmp(bytes.data(), "\xabKTX 20\xbb\r\n\x1a\n", 12) == 0)) {
    auto texture = decodeKTX2(bytes, role, maximumBytes, budget);
    if (mip == rendering::MipPolicy::None && texture->levels().size() > 1)
      return std::make_shared<const rendering::Texture>(
          role, std::vector<rendering::PackedTextureLevel>{
                    texture->levels().front()});
    if (mip == rendering::MipPolicy::Generate &&
        texture->levels().size() == 1) {
      const auto lease = budget.acquire(texture->bytes() * 4);
      return rendering::makeTexture(texture->levels().front(), role, mip,
                                    maximumBytes);
    }
    if (mip != rendering::MipPolicy::None &&
        mip != rendering::MipPolicy::Generate &&
        mip != rendering::MipPolicy::Provided)
      throw std::invalid_argument("Invalid texture mip policy");
    return texture;
  }
  int width{}, height{}, channels{};
  if (bytes.empty() || bytes.size() > INT_MAX || bytes.size() > maximumBytes ||
      !stbi_info_from_memory(reinterpret_cast<const stbi_uc *>(bytes.data()),
                             int(bytes.size()), &width, &height, &channels) ||
      width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
      std::uint64_t(width) * height > maximumBytes / 4)
    throw std::invalid_argument("Invalid or excessive texture dimensions");
  const auto estimate = std::size_t(width) * height * 16 + bytes.size();
  const auto lease = budget.acquire(estimate);
  auto decoded =
      decodeModelImage(bytes, mime, maximumBytes, budget.resources());
  const auto surface =
      std::dynamic_pointer_cast<const SurfacePaintImage>(decoded);
  return rendering::makeTexture(packSurfaceRGBA8(*surface), role, mip,
                                maximumBytes, budget.resources());
}

rendering::TextureHandle decodeHDR(std::span<const std::byte> bytes,
                                   std::size_t maximumBytes,
                                   PreparationBudget &budget,
                                   rendering::TextureFormat storage) {
  if (storage != rendering::TextureFormat::RGBA16F &&
      storage != rendering::TextureFormat::RGBA32F)
    throw std::invalid_argument("HDR decoding requires floating-point storage");
  const auto stride = rendering::texelBytes(storage);
  if (bytes.empty() || bytes.size() > INT_MAX || bytes.size() > maximumBytes)
    throw std::length_error("Invalid HDR input length");
  int w{}, h{}, components{};
  const auto *data = reinterpret_cast<const stbi_uc *>(bytes.data());
  if (!stbi_is_hdr_from_memory(data, int(bytes.size())) ||
      !stbi_info_from_memory(data, int(bytes.size()), &w, &h, &components) ||
      w <= 0 || h <= 0 || w > 16384 || h > 16384 ||
      std::uint64_t(w) * h > maximumBytes / stride)
    throw std::invalid_argument("Invalid or excessive Radiance HDR");
  const auto lease =
      budget.acquire(std::size_t(w) * h * (16 + stride) + bytes.size());
  std::unique_ptr<float, decltype(&stbi_image_free)> pixels{
      stbi_loadf_from_memory(data, int(bytes.size()), &w, &h, &components, 4),
      stbi_image_free};
  if (!pixels)
    throw std::runtime_error("Cannot decode Radiance HDR");
  std::vector<std::byte> packed(std::size_t(w) * h * stride);
  std::array<math::Vec4f, 4096> block;
  for (std::size_t i = 0; i < std::size_t(w) * h; i += block.size()) {
    const auto count = std::min(block.size(), std::size_t(w) * h - i);
    for (std::size_t j = 0; j < count; ++j) {
      const auto *p = pixels.get() + (i + j) * 4;
      block[j] = {p[0], p[1], p[2], p[3]};
    }
    rendering::PackedTexels part{storage, rendering::ColorEncoding::Linear,
                                 std::span{block.data(), count},
                                 budget.resources()};
    std::memcpy(packed.data() + i * stride, part.data().data(), count * stride);
  }
  std::vector<rendering::PackedTextureLevel> levels;
  levels.push_back(
      {{w, h},
       rendering::PackedTexels{storage, rendering::ColorEncoding::Linear,
                               std::move(packed), budget.resources()}});
  return std::make_shared<const rendering::Texture>(
      rendering::TextureRole::Environment, std::move(levels));
}
} // namespace playground::sdl
