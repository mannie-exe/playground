#include <numbers>

#include <platform/sdl/SurfacePainter.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

struct PreparedLeaf : ui::Node {
  bool prepared{};
  math::Vec2f density;
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &) override {
    return {{10, 10}};
  }
  void prepareContent(ui::PrepareContext &context) override {
    prepared = true;
    density = context.pixelScale;
  }
  void paint(rendering::PaintContext &) const override {
    test::require(prepared,
                  "singular subtree must not paint unprepared resources");
  }
};

int main() {
  return test::run([] {
    const math::RoundedRect round{math::rect(0, 0, 20, 20),
                                  math::CornerRadii::all(10)};
    test::require(!round.contains({0, 0}) && round.contains({10, 10}),
                  "rounded geometry");
    test::require(math::CornerRadii::all(40).resolved({20, 20}) ==
                      math::CornerRadii::all(10),
                  "overlapping corners normalize together");
    test::rejects([] { math::CornerRadii::all(-1).validate(); },
                  "negative radius rejected");
    SDLResource<SDL_Surface, SDL_DestroySurface> target{
        SDL_CreateSurface(40, 40, SDL_PIXELFORMAT_RGBA32)};
    auto clear = [&] {
      test::require(SDL_FillSurfaceRect(target.get(), nullptr, 0), "clear");
    };
    auto pixel = [&](int x, int y) {
      math::ColorRGBA8 c;
      test::require(
          SDL_ReadSurfacePixel(target.get(), x, y, &c.r, &c.g, &c.b, &c.a),
          "pixel");
      return c;
    };
    clear();
    {
      sdl::SurfacePainter painter{*target};
      painter.translate({20, 10});
      painter.transform(
          math::Transform2D::rotation(std::numbers::pi_v<float> / 2));
      painter.fill(math::rect(0, 0, 10, 5), {255, 0, 0, 255});
    }
    test::require(pixel(17, 12).r == 255 && pixel(22, 12).a == 0,
                  "rotated fill is not its AABB");
    clear();
    {
      sdl::SurfacePainter painter{*target};
      painter.clipRounded(round);
      painter.beginLayer(math::rect(0, 0, 20, 20), 0.5f);
      painter.fill(math::rect(0, 0, 20, 20), {255, 0, 0, 255});
      painter.fill(math::rect(0, 0, 20, 20), {0, 255, 0, 255});
      painter.endLayer();
    }
    test::require(pixel(0, 0).a == 0 && pixel(10, 10).g >= 127 &&
                      pixel(10, 10).g <= 128,
                  "rounded group clip and opacity applied once");
    ui::UIRoot root;
    auto rectangle =
        std::make_unique<ui::Rectangle>(ui::RectangleProps{{255, 0, 0, 255}});
    rectangle->setHitTestPolicy(ui::HitTestPolicy::Self);
    auto clip = std::make_unique<ui::Clip>(std::move(rectangle));
    clip->setContentAlignment(layout::Alignment::stretch());
    clip->setCornerRadii(math::CornerRadii::all(10));
    root.setContent(std::move(clip));
    root.flushLayout({20, 20});
    test::require(!root.hitTest({0, 0}) && root.hitTest({10, 10}),
                  "rounded clipping gates picking");
    clear();
    {
      sdl::SurfacePainter painter{*target};
      painter.transform(math::Transform2D::scaling({0, 1}));
      painter.fill(math::rect(0, 0, 20, 20), {255, 255, 255, 255});
    }
    test::require(pixel(0, 0).a == 0, "singular transform draws nothing");
    clear();
    {
      SurfaceHandle image{SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_RGBA32),
                          SurfaceHandleDeleter{}};
      SDL_WriteSurfacePixel(image.get(), 0, 0, 255, 0, 0, 255);
      SDL_WriteSurfacePixel(image.get(), 1, 0, 0, 0, 255, 255);
      sdl::SurfacePainter painter{*target};
      painter.translate({20, 5});
      painter.transform(math::Transform2D::scaling({-1, 1}));
      painter.drawImage(sdl::makeSurfaceImage(image), math::rect(0, 0, 2, 1),
                        math::rect(0, 0, 20, 10),
                        {.sampling = rendering::Sampling::Nearest});
    }
    test::require(pixel(5, 10).b == 255 && pixel(15, 10).r == 255,
                  "reflected image maps inverse coordinates");
    clear();
    {
      sdl::SurfacePainter painter{*target};
      painter.save();
      painter.translate({20, 5});
      painter.transform(
          math::Transform2D::rotation(std::numbers::pi_v<float> / 4));
      painter.clip(math::rect(0, 0, 10, 10));
      painter.fill(math::rect(-100, -100, 200, 200), {255, 255, 255, 255});
      painter.restore();
      painter.fill(math::rect(0, 0, 2, 2), {255, 0, 0, 255});
    }
    test::require(pixel(13, 6).a == 0 && pixel(20, 11).a == 255,
                  "rotated clip preserves actual shape, not bounding box");
    test::require(pixel(0, 0).r == 255,
                  "restoring painter removes transformed clip");
    clear();
    {
      ui::UIRoot joined;
      joined.setContent(std::make_unique<ui::Rectangle>(
          ui::RectangleProps{{0, 255, 0, 255},
                             math::ColorRGBA8{255, 0, 0, 255},
                             math::CornerRadii::all(8)},
          layout::BoxProps{.borderWidths = math::Insets::all(3.5f)}));
      joined.flushLayout({20, 20});
      sdl::SurfacePainter painter{*target};
      joined.render(painter);
    }
    test::require(pixel(10, 3).a == 255,
                  "opaque rounded fill/border join has no alpha seam");
    {
      ui::UIRoot singular;
      auto leaf = std::make_unique<PreparedLeaf>();
      leaf->setVisualProps({.transform = math::Transform2D::scaling({0, 1})});
      singular.setContent(std::move(leaf));
      singular.flushLayout({20, 20});
      singular.prepare();
      sdl::SurfacePainter painter{*target};
      singular.render(painter);
    }
    {
      ui::UIRoot densityRoot;
      auto leaf = std::make_unique<PreparedLeaf>();
      auto *probe = leaf.get();
      leaf->setVisualProps({.transform = math::Transform2D::rotation(
                                std::numbers::pi_v<float> / 2)});
      densityRoot.setContent(std::move(leaf));
      densityRoot.flushLayout(
          ui::LayoutEnvironment{.viewport = {20, 20}, .pixelScale = {4, 2}});
      densityRoot.prepare();
      test::require(math::almostEqual(probe->density, math::Vec2f{2, 4}),
                    "raster preparation composes rotation before anisotropic "
                    "device scale");
    }
  });
}
