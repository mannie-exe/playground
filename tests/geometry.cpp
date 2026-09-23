#include <cmath>
#include <exception>
#include <format>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

#include "math/GeometryFormatters.hpp"
#include "platform/sdl/SDLComparisons.hpp"
#include "platform/sdl/SDLFormatters.hpp"
#include "platform/sdl/SDLGeometry.hpp"

namespace math = playground::math;
namespace sdl = playground::sdl;

namespace {

void check(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

template <typename Exception, typename Operation>
void checkThrows(Operation operation, std::string_view message) {
  try {
    operation();
  } catch (const Exception &) {
    return;
  }
  throw std::runtime_error(std::string{message});
}

void testVectorsAndGeometry() {
  static_assert(math::Vec2i{2, 3} + math::Vec2i{4, 5} == math::Vec2i{6, 8});
  static_assert(math::Vec2i{2, 3} * math::Vec2i{4, 5} == math::Vec2i{8, 15});
  static_assert(math::Vec2i{8, 15} / math::Vec2i{4, 5} == math::Vec2i{2, 3});
  static_assert(2 * math::Vec2i{2, 3} == math::Vec2i{4, 6});
  static_assert(math::Vec2f{4, 6} / 2.0f == math::Vec2f{2, 3});
  static_assert(math::Point2{4, 6} - math::Point2{1, 2} == math::Vec2f{3, 4});
  static_assert(math::Point2{1, 2} + math::Vec2f{3, 4} == math::Point2{4, 6});
  static_assert(math::area(math::Vec2i{100000, 100000}) == 10000000000LL);
  check(math::inBounds(math::Vec2i{4, 4}, math::Vec2i{}, math::Vec2i{4, 4}),
        "Inclusive bounds include the maximum");
  check(!math::inBoundsExclusive(math::Vec2i{4, 0}, math::Vec2i{4, 4}),
        "Grid bounds exclude the maximum");
  check(!math::inBoundsExclusive(math::Vec2i{-1, 0}, math::Vec2i{4, 4}),
        "Grid bounds reject negative indices");
  check(!math::hasArea(math::Size2{0, 4}), "Zero width has no area");
  check(!math::isNonNegative(math::Size2{-1, 4}),
        "Negative sizes are identifiable");

  const auto bounds = math::rect(10, 20, 30, 40);
  check(bounds.contains({10, 20}), "Rectangle includes top-left");
  check(!bounds.contains({40, 20}) && !bounds.contains({10, 60}),
        "Rectangle excludes right and bottom edges");
  check(!math::rect(10, 20, 0, 40).contains({10, 20}),
        "Empty rectangle contains no points");
  check(math::inset(bounds, math::Insets::all(5)) == math::rect(15, 25, 20, 30),
        "Insets reduce the content box");
  check(math::outset(bounds, math::Insets::all(5)) == math::rect(5, 15, 40, 50),
        "Outsets enlarge the box");
  check(math::inset(bounds, math::Insets::all(50)).size == math::Size2{},
        "Excess padding never yields negative size");
  check(math::intersect(bounds, math::rect(20, 30, 50, 50)) ==
            math::rect(20, 30, 20, 30),
        "Intersection uses overlapping edges");
  check(!math::intersect(bounds, math::rect(100, 100, 1, 1)).hasArea(),
        "Disjoint intersection is empty");
  check(math::unite(bounds, math::rect(0, 0, 20, 30)) ==
            math::rect(0, 0, 40, 60),
        "Union covers both rectangles");
  check(math::unite(bounds, math::rect(-100, -100, 0, 0)) == bounds,
        "Empty rectangles do not enlarge union");
  check(bounds + math::Vec2f{2, 3} == math::rect(12, 23, 30, 40),
        "Rectangle translation preserves size");
  check(!math::isFinite(
            math::rect(0, 0, std::numeric_limits<float>::infinity(), 1)),
        "Infinite geometry is identifiable");
}

void testTransforms() {
  const auto transform = math::Transform2D::translation({10, 20}) *
                         math::Transform2D::scaling({2, 3});
  check(transform.mapPoint({1, 2}) == math::Point2{12, 26},
        "Composition applies the right operand first");
  check(transform.mapVector({1, 2}) == math::Vec2f{2, 6},
        "Displacements ignore translation");
  const auto inverse = transform.inverse();
  check(inverse.has_value(), "Nonsingular transform has an inverse");
  check(math::almostEqual(inverse->mapPoint(transform.mapPoint({3, 7})),
                          math::Point2{3, 7}),
        "Inverse restores the point");
  check(!math::Transform2D::scaling({0, 2}).inverse(),
        "Singular transform has no inverse");
  check(
      !math::Transform2D::scaling({std::numeric_limits<float>::denorm_min(), 1})
           .inverse(),
      "Inverse outside float range is rejected before narrowing");
  check(!math::Transform2D::translation(
             {std::numeric_limits<float>::infinity(), 0})
             .inverse(),
        "Infinite translation has no usable inverse");
  check(
      !math::Transform2D::scaling({std::numeric_limits<float>::quiet_NaN(), 1})
           .inverse(),
      "NaN transform has no inverse");
  const auto hugeTranslation =
      math::Transform2D::translation({std::numeric_limits<float>::max(), 0}) *
      math::Transform2D::scaling({0.5f, 1});
  check(!hugeTranslation.inverse(),
        "Overflow in inverse translation is rejected");
  const auto rotation =
      math::Transform2D::rotation(std::numbers::pi_v<float> / 2);
  check(math::almostEqual(rotation.mapPoint({1, 0}), math::Point2{0, 1}),
        "Positive rotation maps the x axis to the y axis");
  const auto around =
      math::Transform2D::around({4, 5}, math::Transform2D::scaling({2, 3}));
  check(around.mapPoint({4, 5}) == math::Point2{4, 5},
        "Transform pivot stays fixed");
  check(around.mapPoint({5, 6}) == math::Point2{6, 8},
        "Pivot transform applies to relative position");
  check(math::almostEqual(rotation.mapBounds(math::rect(0, 0, 2, 3)),
                          math::rect(-3, 0, 3, 2)),
        "Rotated bounds cover all four corners");
}

void testSDLConversions() {
  using sdl::PixelRounding;
  check(sdl::fromSDL(sdl::toSDL(math::Vec2i{2, 3})) == math::Vec2i{2, 3},
        "Integer vector conversion round-trips");
  check(sdl::fromSDL(sdl::toSDL(math::Vec2f{0.5f, 2.5f})) ==
            math::Vec2f{0.5f, 2.5f},
        "Float vector conversion round-trips");
  check(sdl::fromSDL(sdl::toSDL(math::rect(1, 2, 3, 4))) ==
            math::rect(1, 2, 3, 4),
        "Float rectangle conversion round-trips");
  check(sdl::fromSDL(SDL_Rect{1, 2, 3, 4}) == math::rect(1, 2, 3, 4),
        "Integer rectangles convert to geometry");
  check(sdl::fromSDL(sdl::toSDL(math::ColorRGBA8{1, 2, 3, 4})) ==
            math::ColorRGBA8{1, 2, 3, 4},
        "Color conversion preserves channels");
  check(sdl::equal(sdl::toSDL(math::Point2{1, 2}), SDL_FPoint{1, 2}),
        "Point conversion preserves position");
  check(sdl::equal(sdl::toSDL(math::Size2{3, 4}), SDL_FPoint{3, 4}),
        "Size conversion preserves dimensions");
  check(sdl::checkedPixel(0.5) == 1 && sdl::checkedPixel(-0.5) == -1,
        "Nearest rounds ties away from zero");
  check(sdl::checkedPixel(-0.2, PixelRounding::Floor) == -1,
        "Floor rounds toward negative infinity");
  check(sdl::checkedPixel(-0.2, PixelRounding::Ceil) == 0,
        "Ceil rounds toward positive infinity");
  check(sdl::checkedPixel(-0.8, PixelRounding::Truncate) == 0,
        "Truncate rounds toward zero");
  check(sdl::checkedPixel(std::numeric_limits<int>::max()) ==
            std::numeric_limits<int>::max(),
        "Largest integer coordinate is accepted");
  check(sdl::checkedPixel(std::numeric_limits<int>::min()) ==
            std::numeric_limits<int>::min(),
        "Smallest integer coordinate is accepted");
  checkThrows<std::out_of_range>(
      [] {
        sdl::checkedPixel(static_cast<double>(std::numeric_limits<int>::max()) +
                          1);
      },
      "Coordinate overflow must throw");
  checkThrows<std::out_of_range>(
      [] {
        sdl::checkedPixel(static_cast<double>(std::numeric_limits<int>::min()) -
                          1);
      },
      "Coordinate underflow must throw");
  checkThrows<std::invalid_argument>(
      [] { sdl::checkedPixel(std::numeric_limits<double>::infinity()); },
      "Infinite coordinates must throw");
  checkThrows<std::invalid_argument>(
      [] { sdl::checkedPixel(std::numeric_limits<double>::quiet_NaN()); },
      "NaN coordinates must throw");
  checkThrows<std::invalid_argument>(
      [] { sdl::checkedPixel(1, PixelRounding::Outward); },
      "Outward policy requires interval edges");
  checkThrows<std::invalid_argument>(
      [] { sdl::checkedPixel(1, static_cast<PixelRounding>(255)); },
      "Unknown rounding policy must throw");
  checkThrows<std::invalid_argument>([] { sdl::toPixelSize({-1, 1}); },
                                     "Negative pixel sizes must throw");
  checkThrows<std::invalid_argument>(
      [] { sdl::toPixelSize({std::numeric_limits<float>::quiet_NaN(), 1}); },
      "NaN pixel sizes must throw");
  checkThrows<std::invalid_argument>(
      [] { sdl::toPixelRect(math::rect(0, 0, -1, 1)); },
      "Negative rectangle extents must throw");
  checkThrows<std::invalid_argument>(
      [] {
        sdl::toPixelRect(
            math::rect(0, 0, std::numeric_limits<float>::infinity(), 1));
      },
      "Infinite rectangle extents must throw");
  checkThrows<std::out_of_range>(
      [] { sdl::toPixelRect(math::rect(-1500000000.0f, 0, 3000000000.0f, 1)); },
      "Rectangle width overflow must throw even if its edges fit");
  check(sdl::equal(sdl::toPixelRect(math::rect(0.2f, -0.2f, 0.6f, 0.6f)),
                   SDL_Rect{0, -1, 1, 2}),
        "Outward rounding covers fractional edges");
  check(sdl::equal(sdl::toPixelRect(math::rect(0.2f, -0.2f, 0, 0)),
                   SDL_Rect{0, -1, 0, 0}),
        "Outward rounding preserves empty size");

  // The exact double sum is below 0.5; the canonical float edge is 0.5.
  const auto first = math::rect(0.4f, 0, 0.09999999f, 1);
  const auto second = math::rect(first.right(), 0, 0.5f, 1);
  const auto firstPixels = sdl::toPixelRect(first, PixelRounding::Nearest);
  const auto secondPixels = sdl::toPixelRect(second, PixelRounding::Nearest);
  check(firstPixels.x + firstPixels.w == secondPixels.x,
        "Adjacent partitions must quantize the same shared float edge");
}

void testFormatting() {
  check(std::format("{}", math::Vec2i{1, 2}) == "Vec2i{1, 2}",
        "Vector formatter identifies its type");
  check(std::format("{}", math::ColorRGBA8{1, 2, 3, 255}) ==
            "ColorRGBA8{1, 2, 3, 255}",
        "Color formatter emits numeric bytes");
  const auto expected = std::format("{:>40}", "Point2{1, 2}");
  check(std::format("{:>40}", math::Point2{1, 2}) == expected,
        "Geometry formatter honors string alignment");
  check(std::format("{:*>30}", SDL_Point{1, 2}) ==
            std::format("{:*>30}", "SDL_Point{1, 2}"),
        "SDL formatter honors fill and alignment");
  check(!std::format("{} {} {} {} {} {} {} {} {} {}", math::Vec2f{1, 2},
                     math::Size2{1, 2}, math::Insets::all(1), math::Gap2{1, 2},
                     math::rect(1, 2, 3, 4), math::Transform2D{},
                     SDL_FPoint{1, 2}, SDL_Rect{1, 2, 3, 4},
                     SDL_FRect{1, 2, 3, 4}, SDL_Color{1, 2, 3, 4})
             .empty(),
        "All geometry and SDL formatters are instantiable");
}

} // namespace

int main() {
  try {
    testVectorsAndGeometry();
    testTransforms();
    testSDLConversions();
    testFormatting();
    std::cout << "Geometry and SDL adapter checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Geometry test failed: " << error.what() << '\n';
    return 1;
  }
}
