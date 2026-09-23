#include <app/TTFGuard.hpp>
#include <memory>
#include <platform/sdl/SurfacePainter.hpp>
#include <string>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Image.hpp>
#include <ui/content/Text.hpp>
#include <ui/content/Vector.hpp>

using namespace playground;

class RecordingPaint final : public ui::PaintContext {
public:
  int depth{}, draws{};
  math::Size2 pixels;
  void save() override { ++depth; }
  void restore() noexcept override { --depth; }
  void translate(math::Vec2f) override {}
  void clip(math::Rect) override {}
  void fill(math::Rect, math::ColorRGBA8) override {}
  void drawImage(const ui::PaintImageHandle &image, math::Rect source,
                 math::Rect destination, ui::ImagePaint) override {
    test::require(math::isFinite(source) && math::isFinite(destination),
                  "finite draw request");
    ++draws;
    pixels = image->pixelSize();
  }
};

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    const std::string base = PLAYGROUND_SOURCE_DIR;
    const auto font = assets.getFont({.path = base + "/assets/fonts/LBRITE.TTF",
                                      .style = {.size = 20},
                                      .layout = {.lineSpace = 24}});
    ui::UIRoot root;
    RecordingPaint paint;
    SDLResource<SDL_Surface, SDL_DestroySurface> target{
        SDL_CreateSurface(80, 60, SDL_PIXELFORMAT_RGBA32)};
    test::require(bool(target), "real painter target allocation");
    int variants{};
    for (auto method : {ui::TextMethod::Blended, ui::TextMethod::Solid,
                        ui::TextMethod::Shaded, ui::TextMethod::LCD})
      for (auto wrap : {ui::TextWrap::None, ui::TextWrap::AvailableInlineSize})
        for (auto fit : {ui::FontFit::None, ui::FontFit::ShrinkToFit})
          for (auto align : {layout::Align::Start, layout::Align::Center,
                             layout::Align::End})
            for (auto direction : {layout::LayoutDirection::LeftToRight,
                                   layout::LayoutDirection::RightToLeft}) {
              auto text = std::make_unique<ui::Text>(
                  assets, ui::TextProps{.value = "alpha beta",
                                        .font = font,
                                        .method = method,
                                        .wrap = wrap,
                                        .paragraphAlignment = align,
                                        .fontFit = fit,
                                        .minFontSize = 8,
                                        .maxFontSize = 20,
                                        .fitStep = 2});
              auto *t = text.get();
              root.setContent(std::move(text));
              root.flushLayout(math::Size2{80, 60}, direction);
              test::rejects<std::logic_error>([&] { root.render(paint); },
                                              "text requires preparation");
              root.prepare();
              const int before = paint.draws;
              root.render(paint);
              test::require(
                  paint.draws == before + 1 && paint.depth == 0,
                  "each text method produces balanced drawable content");
              test::require(SDL_FillSurfaceRect(target.get(), nullptr, 0),
                            "clear text target");
              sdl::SurfacePainter backend{*target};
              root.render(backend);
              bool visible{};
              for (int y = 0; y < target->h && !visible; ++y)
                for (int x = 0; x < target->w && !visible; ++x) {
                  Uint8 r{}, g{}, b{}, a{};
                  test::require(
                      SDL_ReadSurfacePixel(target.get(), x, y, &r, &g, &b, &a),
                      "read rendered pixel");
                  visible = r || g || b;
                }
              test::require(visible, "each text variant produces visible "
                                     "pixels through SDL painter");
              test::require(t->effectiveFont()->getSize() <= 20 &&
                                font->getSize() == 20,
                            "fitting never mutates source font");
              const auto saved = t->props();
              test::rejects(
                  [&] {
                    t->applyPatch({.font = ui::Patch<FontHandle>::reset()});
                  },
                  "required font cannot Reset");
              test::require(t->props() == saved,
                            "rejected text patch preserves props");
              t->setValue("");
              root.flushLayout({80, 60});
              root.prepare();
              const int emptyBefore = paint.draws;
              root.render(paint);
              test::require(paint.draws == emptyBefore,
                            "empty text creates no draw call");
              ++variants;
            }
    test::require(variants == 96,
                  "full selected text mode cross product executed");

    auto surface =
        SurfaceHandle{SDL_CreateSurface(40, 20, SDL_PIXELFORMAT_RGBA32),
                      SurfaceHandleDeleter{}};
    test::require(bool(surface), "image test surface allocated");
    test::require(
        SDL_FillSurfaceRect(surface.get(), nullptr,
                            SDL_MapSurfaceRGBA(surface.get(), 0, 255, 0, 255)),
        "fill image source");
    auto image = std::make_unique<ui::Image>(ui::ImageProps{
        .image = sdl::makeSurfaceImage(surface), .assetDensity = 2});
    auto *i = image.get();
    ui::MeasureContext context;
    test::require(i->measure(context, {}).size == math::Size2{20, 10},
                  "image natural size includes asset density");
    root.setContent(std::move(image));
    root.flushLayout({80, 60});
    root.prepare();
    root.render(paint);
    const auto oldImage = i->props();
    {
      sdl::SurfacePainter backend{*target};
      root.render(backend);
      Uint8 r{}, g{}, b{}, a{};
      test::require(
          SDL_ReadSurfacePixel(target.get(), 40, 30, &r, &g, &b, &a) &&
              r == 0 && g == 255 && b == 0,
          "Image renders expected source color through SDL");
    }
    test::rejects(
        [&] {
          i->applyPatch(
              {.sourceRect = ui::Patch<std::optional<math::Rect>>::set(
                   math::rect(30, 0, 20, 10))});
        },
        "out-of-resource crop rejected");
    test::rejects(
        [&] {
          i->applyPatch({.image = ui::Patch<ui::PaintImageHandle>::reset()});
        },
        "required image cannot Reset");
    test::rejects(
        [&] { i->applyPatch({.assetDensity = ui::Patch<float>::set(0)}); },
        "zero density rejected");
    test::require(i->props() == oldImage, "invalid image changes are atomic");

    const std::string svg = base + "/assets/minesweeper/images/flag.svg";
    auto vector = std::make_unique<ui::Vector>(
        assets, ui::VectorProps{.source = svg,
                                .intrinsicSize = math::Size2{40, 20},
                                .content = {.fit = ui::ContentFit::Stretch}});
    auto *v = vector.get();
    root.setContent(std::move(vector));
    root.flushLayout({80, 60});
    root.prepare();
    test::require(v->rasterSize() == math::Vec2i{80, 60},
                  "vector raster follows final box");
    root.prepare({.pixelScale = {2, 2}});
    test::require(v->rasterSize() == math::Vec2i{160, 120},
                  "vector raster follows pixel density");
    root.render(paint);
    test::rejects<std::length_error>(
        [&] { root.prepare({.pixelScale = {100, 100}}); },
        "density-only change still enforces vector budget");
    test::rejects<std::logic_error>(
        [&] { root.render(paint); },
        "failed density-only preparation cannot reuse a clean flag");
    root.prepare();
    root.render(paint);
    const auto oldVector = v->props();
    {
      sdl::SurfacePainter backend{*target};
      root.render(backend);
    }
    test::rejects<std::runtime_error>(
        [&] {
          v->applyPatch({.source = ui::Patch<VectorSource>::set(
                             base + "/assets/missing.svg")});
        },
        "missing vector file fails");
    test::require(v->props() == oldVector,
                  "failed asset load retains old vector props");
    test::rejects(
        [&] { v->applyPatch({.source = ui::Patch<VectorSource>::reset()}); },
        "required path cannot Reset");
    v->applyPatch({.maximumRasterPixels = ui::Patch<std::size_t>::set(1)});
    test::rejects<std::length_error>(
        [&] { root.prepare(); }, "vector budget enforced before allocation");
    test::rejects<std::logic_error>(
        [&] { root.render(paint); },
        "failed changed vector cannot silently draw stale output");
    v->applyPatch({.maximumRasterPixels = ui::Patch<std::size_t>::reset()});
    root.prepare();
    root.render(paint);
    root.flushLayout({0, 0});
    root.prepare();
    const auto draws = paint.draws;
    root.render(paint);
    test::require(paint.draws == draws, "zero-area vector does not draw");
    root.setContent({});
  });
}
