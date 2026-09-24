#include <cmath>
#include <limits>
#include <numbers>

#include <math/Geometry3D.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::math;

bool close(Vec3f a, Vec3f b) {
  return std::abs(a.x - b.x) < 0.0001f && std::abs(a.y - b.y) < 0.0001f &&
         std::abs(a.z - b.z) < 0.0001f;
}
void identity(const Matrix4 &m) {
  const Matrix4 expected;
  for (std::size_t i = 0; i < 16; ++i)
    test::require(std::abs(m.elements[i] - expected.elements[i]) < 0.0001f,
                  "inverse product is identity");
}
int main() {
  return test::run([] {
    const auto z = axisAngle({0, 0, 1}, std::numbers::pi_v<float> / 2);
    test::require(close(transformDirection(rotation(z), {1, 0, 0}), {0, 1, 0}),
                  "positive Z rotation sends X to Y");
    const auto x = axisAngle({1, 0, 0}, 0.4f);
    const auto composed = rotation(z * x);
    const auto separate = rotation(z) * rotation(x);
    for (std::size_t i = 0; i < 16; ++i)
      test::require(std::abs(composed.elements[i] - separate.elements[i]) <
                        0.00001f,
                    "Hamilton composition matches column-vector matrices");
    for (int i = 1; i <= 30; ++i) {
      const Transform3D transform{{float(i), -2, 3},
                                  axisAngle({1, 2, 3}, i * 0.1f),
                                  {-float(i), 2, 0.5f}};
      const auto m = transform.matrix();
      identity(inverse(m) * m);
      identity(m * inverse(m));
      test::require(
          close(transformPoint(inverse(m), transformPoint(m, {1, 2, 3})),
                {1, 2, 3}),
          "point inverse roundtrip");
    }
    identity(inverse(perspectiveLH(1, 1.5f, 0.1f, 100)) *
             perspectiveLH(1, 1.5f, 0.1f, 100));
    test::require(transformDirection(translation({20, 30, 40}), {1, 2, 3}) ==
                      Vec3f{1, 2, 3},
                  "translation does not affect directions");
    const auto scale = scaling({2, 1, 0.5f});
    const auto normal = transformNormal(scale, {1, 1, 0});
    const auto tangent = transformDirection(scale, {1, -1, 0});
    test::require(std::abs(dot(normal, tangent)) < 0.00001f,
                  "normal stays perpendicular under nonuniform scaling");
    test::rejects([] { inverse(scaling({0, 1, 1})); },
                  "singular inverse rejected");
    test::rejects([] { rotation({0, 0, 0, 0}); }, "zero quaternion rejected");
    test::rejects([] { axisAngle({}, 1); }, "zero axis rejected");
    test::rejects(
        [] { axisAngle({1, 0, 0}, std::numeric_limits<float>::infinity()); },
        "nonfinite angle rejected");
    test::rejects([] { transformNormal(scaling({1, 0, 1}), {0, 1, 0}); },
                  "singular normal transform rejected");
    test::rejects([] { transformPoint(perspectiveLH(1, 1, 1, 10), {}); },
                  "zero W rejected");
    test::rejects(
        [] { transformDirection(perspectiveLH(1, 1, 1, 10), {1, 0, 0}); },
        "projective direction ambiguity rejected");
  });
}
