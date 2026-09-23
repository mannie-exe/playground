#pragma once

#include <cstdint>

namespace playground::math {

struct ColorRGBA8 {
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};
  std::uint8_t a{255};
  constexpr bool operator==(const ColorRGBA8 &) const = default;
};

} // namespace playground::math
