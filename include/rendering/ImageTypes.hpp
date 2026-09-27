#pragma once

namespace playground::rendering {

enum class AlphaMode { Straight, Premultiplied };
enum class ColorEncoding { SRGB, Linear };

// Alpha is always linear coverage. Premultiplied RGB is associated in the
// declared encoding: legacy SRGB-associated bytes must be unpremultiplied
// before decoding; they are NOT encoded linear-premultiplied values.
constexpr bool isValid(AlphaMode value) {
  return value == AlphaMode::Straight || value == AlphaMode::Premultiplied;
}

constexpr bool isValid(ColorEncoding value) {
  return value == ColorEncoding::SRGB || value == ColorEncoding::Linear;
}

} // namespace playground::rendering
