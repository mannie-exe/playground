#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <math/Geometry2D.hpp>
#include <stdexcept>
#include <vector>

namespace playground::rendering {

enum class AlphaMode { Straight, Premultiplied };

// Tightly packed, top-to-bottom RGBA bytes; no implicit gamma conversion.
// GPU uploads currently use RGBA8_UNORM to preserve the software pixel values.
struct RGBA8Image {
  math::Vec2i size;
  AlphaMode alpha{AlphaMode::Straight};
  std::vector<std::uint8_t> pixels;

  static std::size_t byteSize(math::Vec2i size) {
    if (!math::hasArea(size))
      throw std::invalid_argument("Image dimensions must be positive");
    const auto width = static_cast<std::size_t>(size.x);
    const auto height = static_cast<std::size_t>(size.y);
    if (width > std::numeric_limits<std::size_t>::max() / 4 / height)
      throw std::overflow_error("RGBA image exceeds addressable storage");
    return width * height * 4;
  }
  void validate() const {
    if (pixels.size() != byteSize(size))
      throw std::invalid_argument("RGBA byte count does not match dimensions");
    if (alpha != AlphaMode::Straight && alpha != AlphaMode::Premultiplied)
      throw std::invalid_argument("Unknown alpha mode");
  }
};

} // namespace playground::rendering
