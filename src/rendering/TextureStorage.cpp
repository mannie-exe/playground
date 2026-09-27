#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <rendering/Texture.hpp>

namespace playground::rendering {
namespace {
float decodeSRGB(float x) {
  return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
}

float encodeSRGB(float x) {
  return x <= .0031308f ? 12.92f * x : 1.055f * std::pow(x, 1 / 2.4f) - .055f;
}

std::uint16_t half(float value) {
  if (!std::isfinite(value) || std::abs(value) > 65504)
    throw std::length_error("Texture exceeds half-float range");
  const auto bits = std::bit_cast<std::uint32_t>(value);
  const auto sign = (bits >> 16) & 0x8000;
  const int exponent = int((bits >> 23) & 255) - 127;
  if (exponent < -25)
    return std::uint16_t(sign);
  const auto mantissa = (bits & 0x7fffff) | 0x800000;
  const unsigned shift = exponent < -14 ? unsigned(-exponent - 1) : 13;
  auto rounded = mantissa >> shift;
  const auto remainder = mantissa & ((1u << shift) - 1);
  const auto midpoint = 1u << (shift - 1);
  rounded += remainder > midpoint || (remainder == midpoint && (rounded & 1));
  return std::uint16_t(
      sign |
      (exponent < -14 ? rounded : unsigned(exponent + 14) * 1024 + rounded));
}

float unhalf(std::uint16_t h) {
  const int exponent = (h >> 10) & 31;
  const unsigned fraction = h & 1023;
  const float magnitude =
      exponent == 0    ? std::ldexp(float(fraction), -24)
      : exponent == 31 ? (fraction ? std::numeric_limits<float>::quiet_NaN()
                                   : std::numeric_limits<float>::infinity())
                       : std::ldexp(float(1024 + fraction), exponent - 25);
  return h & 0x8000 ? -magnitude : magnitude;
}
} // namespace

std::size_t texelBytes(TextureFormat format) {
  switch (format) {
  case TextureFormat::R8:
    return 1;
  case TextureFormat::RGBA8:
    return 4;
  case TextureFormat::RGBA16F:
    return 8;
  case TextureFormat::RGBA32F:
    return 16;
  }
  throw std::invalid_argument("Unknown texture storage format");
}

PackedTexels::PackedTexels(TextureFormat format, ColorEncoding encoding,
                           std::vector<std::byte> data)
    : _format{format}, _encoding{encoding}, _data{std::move(data)} {
  if (!isValid(encoding) || _data.size() % texelBytes(format) ||
      (encoding == ColorEncoding::SRGB && format != TextureFormat::RGBA8))
    throw std::invalid_argument("Invalid packed texture layout/encoding");
}

PackedTexels::PackedTexels(TextureFormat format, ColorEncoding encoding,
                           std::span<const math::Vec4f> linear)
    : PackedTexels{format, encoding, std::vector<std::byte>{}} {
  const auto stride = texelBytes(format);
  if (linear.size() > std::numeric_limits<std::size_t>::max() / stride)
    throw std::length_error("Texture byte count overflow");
  _data.resize(linear.size() * stride);
  for (std::size_t i = 0; i < linear.size(); ++i) {
    const auto p = linear[i];
    if (!math::isFinite(p))
      throw std::invalid_argument("Nonfinite texture input");
    float values[]{p.x, p.y, p.z, p.w};
    if (encoding == ColorEncoding::SRGB)
      for (int c = 0; c < 3; ++c)
        values[c] = encodeSRGB(values[c]);
    for (unsigned c = 0; c < (format == TextureFormat::R8 ? 1u : 4u); ++c) {
      auto *dst = _data.data() + i * stride;
      if (format == TextureFormat::RGBA8 || format == TextureFormat::R8) {
        if (values[c] < 0 || values[c] > 1.000001f)
          throw std::length_error("Texture exceeds normalized-byte range");
        dst[c] = std::byte(std::clamp(std::lround(values[c] * 255), 0l, 255l));
      } else if (format == TextureFormat::RGBA16F) {
        const auto h = half(values[c]);
        std::memcpy(dst + c * 2, &h, 2);
      } else
        std::memcpy(dst + c * 4, &values[c], 4);
    }
  }
}

std::size_t PackedTexels::size() const noexcept {
  const auto stride = _format == TextureFormat::R8        ? 1
                      : _format == TextureFormat::RGBA8   ? 4
                      : _format == TextureFormat::RGBA16F ? 8
                                                          : 16;
  return _data.size() / stride;
}

math::Vec4f PackedTexels::operator[](std::size_t index) const {
  if (index >= size())
    throw std::out_of_range("Texture texel index");
  float v[]{0, 0, 0, 1};
  const auto *src = _data.data() + index * texelBytes(_format);
  for (unsigned c = 0; c < (_format == TextureFormat::R8 ? 1u : 4u); ++c) {
    if (_format == TextureFormat::RGBA8 || _format == TextureFormat::R8)
      v[c] = std::to_integer<unsigned>(src[c]) / 255.f;
    else if (_format == TextureFormat::RGBA16F) {
      std::uint16_t h;
      std::memcpy(&h, src + c * 2, 2);
      v[c] = unhalf(h);
    } else
      std::memcpy(&v[c], src + c * 4, 4);
  }
  if (_encoding == ColorEncoding::SRGB)
    for (int c = 0; c < 3; ++c)
      v[c] = decodeSRGB(v[c]);
  return {v[0], v[1], v[2], v[3]};
}

bool PackedTexels::operator==(const PackedTexels &other) const {
  return _format == other._format && _encoding == other._encoding &&
         _data == other._data;
}
} // namespace playground::rendering
