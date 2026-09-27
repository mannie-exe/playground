#include <iostream>
#include <stdexcept>

#include <SDL3/SDL.h>

#include <layout/LayoutFormatters.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <ui/UIFormatters.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/content/Image.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/content/Text.hpp>
#include <ui/content/Vector.hpp>

static void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int main() {
  try {
    using namespace playground;
    check(std::format("{} {}", ui::TextWrap::AvailableInlineSize,
                      layout::Axis::Horizontal) ==
              "AvailableInlineSize Horizontal",
          "enum formatters");
    check(std::format("{}", ui::DirtyFlags::Paint | ui::DirtyFlags::HitTest) ==
              "Paint|HitTest",
          "compound dirty flag formatter");
    SDLResource<SDL_Surface, SDL_DestroySurface> target{
        SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32)};
    check(bool(target), "create target");
    SDL_FillSurfaceRect(target.get(), nullptr, 0);
    {
      sdl::SurfacePainter painter{*target};
      painter.fill(math::rect(0, 0, 32, 32), {0, 0, 0, 255});
      painter.save();
      painter.clip(math::rect(0, 0, 8, 8));
      painter.fill(math::rect(0, 0, 32, 32), {255, 0, 0, 255});
      painter.restore();
      Uint8 r, g, b, a;
      check(SDL_ReadSurfacePixel(target.get(), 10, 10, &r, &g, &b, &a) &&
                r == 0,
            "clip preserved");
      painter.beginLayer(math::rect(0, 0, 32, 32), 0.5f);
      painter.fill(math::rect(0, 0, 20, 20), {255, 0, 0, 255});
      painter.fill(math::rect(10, 0, 20, 20), {0, 255, 0, 255});
      painter.endLayer();
      check(SDL_ReadSurfacePixel(target.get(), 15, 15, &r, &g, &b, &a) &&
                r < 5 && g >= 126 && g <= 129,
            "group alpha composites once");
      painter.transform(math::Transform2D::rotation(0.3f));
      painter.fill(math::rect(0, 0, 4, 4), {255, 255, 255, 255});
    }
    ui::UIRoot root;
    root.setContent(
        std::make_unique<ui::Rectangle>(ui::RectangleProps{{1, 2, 3, 255}}));
    root.flushLayout({32, 32});
    root.prepare();
    {
      sdl::SurfacePainter painter{*target};
      root.render(painter);
    }
    Uint8 r, g, b, a;
    check(SDL_ReadSurfacePixel(target.get(), 15, 15, &r, &g, &b, &a) &&
              r == 1 && g == 2 && b == 3,
          "retained rectangle painting");
    auto rectangle = std::make_unique<ui::Rectangle>(
        ui::RectangleProps{{200, 0, 0, 255}},
        layout::BoxProps{.width = layout::SizeRule::fill(),
                         .height = layout::SizeRule::fill()});
    auto *child = rectangle.get();
    auto layer = std::make_unique<ui::Layer>(
        std::move(rectangle),
        ui::LayerProps{.cachePolicy = ui::LayerCachePolicy::WhenUnchanged});
    auto *layerPtr = layer.get();
    root.setContent(std::move(layer));
    root.flushLayout({32, 32});
    root.prepare();
    {
      sdl::SurfacePainter painter{*target};
      root.render(painter);
    }
    check(layerPtr->estimatedCacheBytes() > 0,
          "retained layer allocates cache");
    child->setProps({{0, 100, 0, 255}});
    root.prepare();
    {
      sdl::SurfacePainter painter{*target};
      root.render(painter);
    }
    check(SDL_ReadSurfacePixel(target.get(), 5, 5, &r, &g, &b, &a) && r == 0 &&
              g == 100,
          "child paint invalidates layer cache");
    check(TTF_Init(), "TTF init");
    {
      AssetRegistry assets;
      const std::string base = PLAYGROUND_SOURCE_DIR;
      auto font = assets.getFont({.path = base + "/assets/fonts/LBRITE.TTF",
                                  .style = {.size = 24},
                                  .layout = {.lineSpace = 28}});
      auto text = std::make_unique<ui::Text>(
          assets, ui::TextProps{.value = "hello world hello world",
                                .font = font,
                                .wrap = ui::TextWrap::AvailableInlineSize});
      auto *textPtr = text.get();
      root.setContent(std::move(text));
      root.flushLayout({32, 32});
      root.prepare();
      {
        sdl::SurfacePainter painter{*target};
        root.render(painter);
      }
      check(assets.estimatedSurfaceBytes() > 0, "text raster cached");
      textPtr->setValue("");
      root.flushLayout({32, 32});
      root.prepare();
      {
        sdl::SurfacePainter painter{*target};
        root.render(painter);
      }
      root.setContent({});
      assets.clear();
    }
    TTF_Quit();
    std::cout << "Content/painter tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
