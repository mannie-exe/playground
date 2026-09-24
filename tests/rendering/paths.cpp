#include <math/Path2D.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Path.hpp>

using namespace playground;

int main() {
  return test::run([] {
    math::Path2D ring;
    ring.moveTo({1, 1})
        .lineTo({15, 1})
        .lineTo({15, 15})
        .lineTo({1, 15})
        .close()
        .moveTo({5, 5})
        .lineTo({11, 5})
        .lineTo({11, 11})
        .lineTo({5, 11})
        .close();
    const auto flat = math::flattenPath(ring);
    test::require(flat.segments.size() == 8,
                  "closed contours do not duplicate closing edges");
    test::require(flat.contains({8, 8}, math::FillRule::NonZero) &&
                      !flat.contains({8, 8}, math::FillRule::EvenOdd),
                  "fill rules distinguish same-winding hole");
    test::require(!flat.contains({0, 0}, math::FillRule::EvenOdd),
                  "outside path");
    math::Path2D curve;
    curve.moveTo({0, 0}).cubicTo({0, 10}, {10, 10}, {10, 0});
    const auto closed = math::flattenPath(curve);
    const auto open = math::flattenPath(curve, {.closeOpenContours = false});
    test::require(closed.segments.size() == open.segments.size() + 1,
                  "fills close open contours; strokes do not");
    test::require(closed.contains({5, 3}, math::FillRule::NonZero),
                  "cubic curve interior");
    test::rejects<std::length_error>(
        [&] {
          math::flattenPath(curve, {.tolerance = .01f, .maximumSegments = 2});
        },
        "bounded curve subdivision");
    math::Path2D invalid;
    invalid.lineTo({1, 1});
    test::rejects([&] { math::flattenPath(invalid); }, "requires moveTo");
    math::Path2D quadratic;
    quadratic.moveTo({0, 0}).quadraticTo({5, 10}, {10, 0});
    test::require(
        math::flattenPath(quadratic).contains({5, 2}, math::FillRule::NonZero),
        "quadratic interior");
    math::Path2D line;
    line.moveTo({2, 2}).lineTo({10, 2});
    auto segment = math::flattenPath(line, {.closeOpenContours = false});
    test::require(segment.touchesStroke({1.5f, 2}, 1) &&
                      !segment.touchesStroke({0, 2}, 1),
                  "round cap radius");

    SDLResource<SDL_Surface, SDL_DestroySurface> surface{
        SDL_CreateSurface(20, 20, SDL_PIXELFORMAT_RGBA32)};
    test::require(bool(surface), "surface allocation");
    SDL_ClearSurface(surface.get(), 0, 0, 0, 0);
    sdl::SurfacePainter painter{*surface};
    ui::UIRoot root;
    root.setContent(std::make_unique<ui::Path>(
        ui::PathProps{.path = ring,
                      .viewBox = {{}, {20, 20}},
                      .paint = {.fill = math::ColorRGBA8{255, 0, 0, 255},
                                .fillRule = math::FillRule::EvenOdd}}));
    root.flushLayout({20, 20});
    root.prepare();
    root.render(painter);
    Uint8 r{}, g{}, b{}, a{};
    SDL_ReadSurfacePixel(surface.get(), 8, 8, &r, &g, &b, &a);
    test::require(a == 0, "path hole remains transparent");
    SDL_ReadSurfacePixel(surface.get(), 3, 3, &r, &g, &b, &a);
    test::require(r == 255 && a == 255, "path fill through retained UI node");
    painter.drawPath(line, {.fill = std::nullopt,
                            .stroke = math::ColorRGBA8{0, 255, 0, 255},
                            .strokeWidth = 2});
    SDL_ReadSurfacePixel(surface.get(), 5, 2, &r, &g, &b, &a);
    test::require(g == 255, "path stroke rendering");
  });
}
