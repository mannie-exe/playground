#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <ktx.h>

#include <platform/sdl/TextureDecode.hpp>

namespace playground::sdl {
namespace {
void check(KTX_error_code code) {
  if (code != KTX_SUCCESS)
    throw std::invalid_argument(std::string{"KTX2: "} + ktxErrorString(code));
}

std::uint32_t word(std::span<const std::byte> bytes, std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 4)
    throw std::invalid_argument("Truncated KTX2 header");
  std::uint32_t result{};
  for (unsigned i = 0; i < 4; ++i)
    result |= std::uint32_t(std::to_integer<unsigned>(bytes[offset + i]))
              << (i * 8);
  return result;
}
} // namespace

rendering::TextureHandle decodeKTX2(std::span<const std::byte> bytes,
                                    rendering::TextureRole role,
                                    std::size_t maximumBytes,
                                    PreparationBudget &budget) {
  using namespace rendering;
  constexpr std::array<unsigned char, 12> signature{
      0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
  if (bytes.size() < 104 || bytes.size() > maximumBytes ||
      std::memcmp(bytes.data(), signature.data(), signature.size()) != 0)
    throw std::invalid_argument("Invalid/excessive KTX2 input");
  const auto width = word(bytes, 20), height = word(bytes, 24);
  const auto levelCount = std::max(1u, word(bytes, 40));
  if (!width || !height || width > 16384 || height > 16384 || word(bytes, 28) ||
      word(bytes, 32) || word(bytes, 36) != 1 || levelCount > 15)
    throw std::invalid_argument(
        "KTX2 requires bounded 2D non-array single-face content");
  TextureFormat format;
  const auto vkFormat = word(bytes, 12);
  switch (vkFormat) {
  case 0:
  case 37:
  case 43:
    format = TextureFormat::RGBA8;
    break;
  case 9:
    format = TextureFormat::R8;
    break;
  case 97:
    format = TextureFormat::RGBA16F;
    break;
  case 109:
    format = TextureFormat::RGBA32F;
    break;
  default:
    throw std::invalid_argument(
        "Unsupported KTX2 storage format; provide RGBA, R8 or Basis content");
  }
  std::size_t estimated{};
  auto w = width, h = height;
  for (unsigned i = 0; i < levelCount; ++i) {
    const auto count = std::size_t(w) * h;
    if (count > (maximumBytes - estimated) / texelBytes(format))
      throw std::length_error("KTX2 decoded mip chain exceeds budget");
    estimated += count * texelBytes(format);
    if (w == 1 && h == 1 && i + 1 < levelCount)
      throw std::invalid_argument("Excess KTX2 mip levels");
    w = std::max(1u, w / 2);
    h = std::max(1u, h / 2);
  }
  if (estimated > (std::numeric_limits<std::size_t>::max() - bytes.size()) / 4)
    throw std::length_error("KTX2 preparation estimate overflow");
  const auto lease = budget.acquire(bytes.size() + estimated * 4);
  ktxTexture2 *raw{};
  check(ktxTexture2_CreateFromMemory(
      reinterpret_cast<const ktx_uint8_t *>(bytes.data()), bytes.size(),
      KTX_TEXTURE_CREATE_NO_FLAGS, &raw));
  std::unique_ptr<ktxTexture2, decltype(&ktxTexture2_Destroy)> texture{
      raw, ktxTexture2_Destroy};
  if (raw->numDimensions != 2 || raw->isArray || raw->numFaces != 1 ||
      raw->isCubemap || raw->orientation.x != KTX_ORIENT_X_RIGHT ||
      raw->orientation.y != KTX_ORIENT_Y_DOWN ||
      ktxTexture2_GetPremultipliedAlpha(raw))
    throw std::invalid_argument(
        "Unsupported KTX2 shape/orientation/associated alpha");
  unsigned swizzleLength{};
  void *swizzle{};
  if (ktxHashList_FindValue(&raw->kvDataHead, "KTXswizzle", &swizzleLength,
                            &swizzle) == KTX_SUCCESS &&
      (swizzleLength < 4 || std::memcmp(swizzle, "rgba", 4)))
    throw std::invalid_argument(
        "KTX2 channel swizzle must be baked before import");
  const auto transfer = ktxTexture2_GetOETF_e(raw);
  if (transfer != KHR_DF_TRANSFER_LINEAR && transfer != KHR_DF_TRANSFER_SRGB)
    throw std::invalid_argument("Unsupported KTX2 transfer function");
  const auto encoding = transfer == KHR_DF_TRANSFER_SRGB
                            ? ColorEncoding::SRGB
                            : ColorEncoding::Linear;
  const auto primaries = ktxTexture2_GetPrimaries_e(raw);
  if (primaries != KHR_DF_PRIMARIES_UNSPECIFIED &&
      primaries != KHR_DF_PRIMARIES_BT709)
    throw std::invalid_argument(
        "KTX2 primaries require explicit color-space conversion");
  if (vkFormat && ((vkFormat == 43) != (encoding == ColorEncoding::SRGB)))
    throw std::invalid_argument("KTX2 format and transfer metadata disagree");
  if (encoding == ColorEncoding::SRGB && role != TextureRole::Color &&
      role != TextureRole::Emission)
    throw std::invalid_argument("Numerical KTX2 content must be linear");
  if (ktxTexture_GetDataSizeUncompressed(ktxTexture(raw)) > maximumBytes)
    throw std::length_error("KTX2 inflated payload exceeds budget");
  check(ktxTexture2_LoadImageData(raw, nullptr, 0));
  if (ktxTexture2_NeedsTranscoding(raw))
    check(ktxTexture2_TranscodeBasis(raw, KTX_TTF_RGBA32, 0));
  else if (vkFormat == 0)
    throw std::invalid_argument(
        "Undefined KTX2 format is not transcodable Basis data");
  std::vector<PackedTextureLevel> levels;
  w = width;
  h = height;
  const auto dataSize = ktxTexture_GetDataSize(ktxTexture(raw));
  if (dataSize > maximumBytes)
    throw std::length_error("KTX2 transcoded payload exceeds budget");
  for (unsigned i = 0; i < levelCount; ++i) {
    ktx_size_t offset{};
    check(ktxTexture2_GetImageOffset(raw, i, 0, 0, &offset));
    const auto length = std::size_t(w) * h * texelBytes(format);
    if (offset > dataSize || length > dataSize - offset)
      throw std::invalid_argument("KTX2 mip exceeds loaded storage");
    std::vector<std::byte> pixels(length);
    std::memcpy(pixels.data(), raw->pData + offset, length);
    if (role == TextureRole::Emission && format == TextureFormat::RGBA8)
      for (std::size_t p = 3; p < pixels.size(); p += 4)
        pixels[p] = std::byte{255};
    levels.push_back(
        {{int(w), int(h)}, PackedTexels{format, encoding, std::move(pixels)}});
    w = std::max(1u, w / 2);
    h = std::max(1u, h / 2);
  }
  return std::make_shared<const Texture>(role, std::move(levels));
}
} // namespace playground::sdl
