#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <math/ColorSpace.hpp>

namespace playground::math {
namespace {
void unit(float value) {
  if (!std::isfinite(value) || value < 0 || value > 1)
    throw std::invalid_argument("Color channel must be finite and in [0,1]");
}
} // namespace

void LinearRGBA::validate() const {
  unit(r);
  unit(g);
  unit(b);
  unit(a);
}
void PremultipliedRGBA::validate() const {
  unit(r);
  unit(g);
  unit(b);
  unit(a);
  if (r > a || g > a || b > a)
    throw std::invalid_argument("Premultiplied SDR color exceeds alpha");
}
float decodeSRGB(float encoded) {
  unit(encoded);
  return encoded <= 0.04045f ? encoded / 12.92f
                             : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}
float encodeSRGB(float linear) {
  unit(linear);
  return std::clamp(linear <= 0.0031308f
                        ? 12.92f * linear
                        : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f,
                    0.0f, 1.0f);
}
LinearRGBA toLinear(ColorRGBA8 encoded) {
  return {decodeSRGB(encoded.r / 255.0f), decodeSRGB(encoded.g / 255.0f),
          decodeSRGB(encoded.b / 255.0f), encoded.a / 255.0f};
}
ColorRGBA8 toSRGB(LinearRGBA linear) {
  linear.validate();
  const auto byte = [](float value) {
    return static_cast<std::uint8_t>(std::lround(value * 255));
  };
  return {byte(encodeSRGB(linear.r)), byte(encodeSRGB(linear.g)),
          byte(encodeSRGB(linear.b)), byte(linear.a)};
}
PremultipliedRGBA premultiply(LinearRGBA straight) {
  straight.validate();
  return {straight.r * straight.a, straight.g * straight.a,
          straight.b * straight.a, straight.a};
}
LinearRGBA unpremultiply(PremultipliedRGBA associated) {
  associated.validate();
  if (associated.a == 0)
    return {0, 0, 0, 0};
  return {associated.r / associated.a, associated.g / associated.a,
          associated.b / associated.a, associated.a};
}
PremultipliedRGBA sourceOver(PremultipliedRGBA source,
                             PremultipliedRGBA destination) {
  source.validate();
  destination.validate();
  const float remaining = 1 - source.a;
  const float alpha = std::min(1.0f, source.a + destination.a * remaining);
  // Clamp only accumulated floating-point roundoff; input is validated above.
  return {std::min(alpha, source.r + destination.r * remaining),
          std::min(alpha, source.g + destination.g * remaining),
          std::min(alpha, source.b + destination.b * remaining), alpha};
}

} // namespace playground::math
