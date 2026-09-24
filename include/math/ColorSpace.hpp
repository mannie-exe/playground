#pragma once

#include <math/Color.hpp>

namespace playground::math {

// SDR linear-light channels and linear coverage, each in [0,1].
struct LinearRGBA {
  float r{}, g{}, b{}, a{1};
  void validate() const;
  bool operator==(const LinearRGBA &) const = default;
};

// A different type prevents accidentally compositing straight RGB as
// premultiplied.
struct PremultipliedRGBA {
  float r{}, g{}, b{}, a{};
  void validate() const;
  bool operator==(const PremultipliedRGBA &) const = default;
};

float decodeSRGB(float encoded);
float encodeSRGB(float linear);
LinearRGBA toLinear(ColorRGBA8 encoded);
ColorRGBA8 toSRGB(LinearRGBA linear);
PremultipliedRGBA premultiply(LinearRGBA straight);
LinearRGBA unpremultiply(PremultipliedRGBA associated);
PremultipliedRGBA sourceOver(PremultipliedRGBA source,
                             PremultipliedRGBA destination);

} // namespace playground::math
