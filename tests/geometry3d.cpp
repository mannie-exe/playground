#include <cmath>
#include <limits>
#include <math/Geometry3D.hpp>
#include <numbers>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::math;

static bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }

int main() {
  return test::run([] {
    const Matrix4 identity;
    const auto model = translation({3, 4, 5}) * scaling({2, 3, 4});
    test::require(identity * model == model && model * identity == model,
                  "identity on either side");
    const auto point = model * Vec4f{1, 2, 3, 1};
    test::require(point == Vec4f{5, 10, 17, 1},
                  "column-vector scale then translate");
    test::require(translation({3, 4, 5}) * Vec4f{1, 2, 3, 0} ==
                      Vec4f{1, 2, 3, 0},
                  "directions are not translated");
    test::require(
        cross({1, 0, 0}, {0, 1, 0}) == Vec3f{0, 0, 1} &&
            near(dot(normalized({3, 4, 0}), normalized({3, 4, 0})), 1),
        "cross product and normalization");
    const auto perspective =
        perspectiveLH(std::numbers::pi_v<float> / 2, 2, 1, 100);
    for (float depth : {1.0f, 100.0f}) {
      const auto clip = perspective * Vec4f{0, 0, depth, 1};
      test::require(near(clip.z / clip.w, depth == 1 ? 0.0f : 1.0f),
                    "perspective near/far map to zero/one");
    }
    const auto edge = perspective * Vec4f{2, 1, 1, 1};
    test::require(near(edge.x / edge.w, 1) && near(edge.y / edge.w, 1),
                  "aspect ratio and vertical field of view");
    const auto ortho = orthographicLH(4, 2, 0, 10) * Vec4f{2, 1, 10, 1};
    test::require(ortho == Vec4f{1, 1, 1, 1}, "orthographic extents and depth");
    const auto view = lookAtLH({3, 4, 5}, {3, 4, 6});
    test::require(view * Vec4f{3, 4, 5, 1} == Vec4f{0, 0, 0, 1} &&
                      view * Vec4f{3, 4, 6, 1} == Vec4f{0, 0, 1, 1},
                  "camera origin and forward basis");
    test::rejects([] { normalized({}); }, "zero normal rejected");
    test::rejects([] { lookAtLH({}, {}); },
                  "coincident eye and target rejected");
    test::rejects([] { lookAtLH({}, {0, 1, 0}); }, "parallel up rejected");
    test::rejects([] { perspectiveLH(1, 0, 1, 10); }, "zero aspect rejected");
    test::rejects([] { perspectiveLH(1, 1, 0, 10); },
                  "zero perspective near rejected");
    test::rejects([] { orthographicLH(1, 1, 10, 1); },
                  "reversed planes rejected");
    test::rejects(
        [] { translation({std::numeric_limits<float>::infinity(), 0, 0}); },
        "nonfinite translation rejected");
    test::rejects<std::out_of_range>([&] { identity.at(4, 0); },
                                     "row bounds independent of column");
    test::rejects<std::overflow_error>(
        [] {
          scaling({std::numeric_limits<float>::max(), 1, 1}) *
              Vec4f{2, 0, 0, 1};
        },
        "matrix arithmetic cannot silently overflow");
  });
}
