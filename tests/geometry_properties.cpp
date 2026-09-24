#include <array>
#include <cmath>

#include <math/Geometry2D.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    for (int seed = 1; seed <= 250; ++seed) {
      const float x = static_cast<float>(seed % 31 - 15);
      const float y = static_cast<float>(seed % 23 - 11);
      const auto a = math::rect(x, y, seed % 19 + 1.f, seed % 13 + 1.f);
      const auto b = math::rect(y, x, seed % 17 + 1.f, seed % 7 + 1.f);
      const auto intersection = math::intersect(a, b);
      test::require(intersection == math::intersect(b, a),
                    "intersection is commutative");
      test::require(math::unite(a, b) == math::unite(b, a),
                    "union is commutative");
      test::require(math::intersect(a, a) == a && math::unite(a, a) == a,
                    "set operations idempotent");
      test::require(intersection.w() >= 0 && intersection.h() >= 0,
                    "intersection nonnegative");
      const auto inset = math::Insets::all(3);
      test::require(math::inset(math::outset(a, inset), inset) == a,
                    "outset then inset round-trips without clamping");
      const auto transform =
          math::Transform2D::translation({x, y}) *
          math::Transform2D::rotation(seed * 0.07f) *
          math::Transform2D::scaling({seed % 5 + 0.5f, seed % 7 + 0.5f});
      const auto inverse = transform.inverse();
      test::require(inverse.has_value(),
                    "positive finite scales are invertible");
      const auto p = math::Point2{x, y};
      const auto restored = inverse->mapPoint(transform.mapPoint(p));
      test::require(std::abs(restored.x - p.x) < 0.0001 &&
                        std::abs(restored.y - p.y) < 0.0001,
                    "affine inverse round-trip (explicit numerical tolerance)");
      const auto bounds = transform.mapBounds(a);
      for (auto corner : std::array{math::Point2{a.left(), a.top()},
                                    math::Point2{a.right(), a.top()},
                                    math::Point2{a.left(), a.bottom()},
                                    math::Point2{a.right(), a.bottom()}}) {
        const auto mapped = transform.mapPoint(corner);
        test::require(mapped.x >= bounds.left() - 0.0001 &&
                          mapped.x <= bounds.right() + 0.0001 &&
                          mapped.y >= bounds.top() - 0.0001 &&
                          mapped.y <= bounds.bottom() + 0.0001,
                      "mapped AABB encloses all transformed corners");
      }
    }
  });
}
