#include <cmath>
#include <limits>

#include <math/ColorSpace.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::math;

int main() {
  return test::run([] {
    for (int i = 0; i < 256; ++i) {
      const auto channel = static_cast<std::uint8_t>(i);
      const ColorRGBA8 value{channel, channel, channel, channel};
      test::require(toSRGB(toLinear(value)) == value,
                    "all 8-bit channels roundtrip including alpha");
    }
    const auto red = premultiply({1, 0, 0, 0.5f});
    const auto blue = premultiply({0, 0, 1, 1});
    const auto composite = sourceOver(red, blue);
    test::require(composite == PremultipliedRGBA{0.5f, 0, 0.5f, 1},
                  "source-over in linear space");
    const auto encoded = toSRGB(unpremultiply(composite));
    test::require(encoded.r == 188 && encoded.b == 188,
                  "linear mixing is not encoded-byte averaging");
    test::require(sourceOver({}, red) == red && sourceOver(red, {}) == red,
                  "transparent is composition identity");
    test::require(unpremultiply({}) == LinearRGBA{0, 0, 0, 0},
                  "zero alpha has defined zero RGB");
    const LinearRGBA straight{0.25f, 0.5f, 1, 0.5f};
    test::require(unpremultiply(premultiply(straight)) == straight,
                  "premultiplication roundtrip");
    for (float value : {-1.0f, 1.01f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
      test::rejects([&] { decodeSRGB(value); },
                    "invalid encoded channel rejected");
      test::rejects([&] { encodeSRGB(value); },
                    "invalid linear channel rejected");
      test::rejects([&] { premultiply({0, 0, 0, value}); },
                    "invalid alpha rejected");
    }
    test::rejects([] { unpremultiply({0.6f, 0, 0, 0.5f}); },
                  "invalid associated color rejected");
    for (int i = 0; i <= 100; ++i) {
      const float x = i / 100.0f;
      test::require(std::abs(decodeSRGB(encodeSRGB(x)) - x) < 0.000001f,
                    "continuous transfer roundtrip tolerance");
      sourceOver(premultiply({x, 1 - x, 0, x}), red).validate();
    }
  });
}
