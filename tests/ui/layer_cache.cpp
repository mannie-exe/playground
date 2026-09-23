#include <platform/sdl/SurfacePainter.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/content/Rectangle.hpp>

int main() {
  return playground::test::run([] {
    using namespace playground;
    SDLResource<SDL_Surface, SDL_DestroySurface> surface{
        SDL_CreateSurface(40, 20, SDL_PIXELFORMAT_RGBA32)};
    test::require(bool(surface), "surface allocation");
    auto row = std::make_unique<ui::HStack>();
    row->append(std::make_unique<ui::Rectangle>(
        ui::RectangleProps{{255, 0, 0, 255}},
        layout::BoxProps{.width = layout::SizeRule::fixed(20),
                         .height = layout::SizeRule::fixed(20)}));
    row->append(std::make_unique<ui::Rectangle>(
        ui::RectangleProps{{0, 255, 0, 255}},
        layout::BoxProps{.width = layout::SizeRule::fixed(20),
                         .height = layout::SizeRule::fixed(20)}));
    auto layer = std::make_unique<ui::Layer>(
        std::move(row),
        ui::LayerProps{.cachePolicy = ui::LayerCachePolicy::WhenUnchanged});
    auto *layerPtr = layer.get();
    ui::UIRoot root;
    root.setContent(std::move(layer));
    const auto render = [&](layout::LayoutDirection direction) {
      root.flushLayout(math::Size2{40, 20}, direction);
      root.prepare();
      sdl::SurfacePainter painter{*surface};
      root.render(painter);
    };
    render(layout::LayoutDirection::LeftToRight);
    Uint8 r{}, g{}, b{}, a{};
    test::require(SDL_ReadSurfacePixel(surface.get(), 5, 5, &r, &g, &b, &a) &&
                      r == 255,
                  "LTR layer starts red");
    render(layout::LayoutDirection::RightToLeft);
    test::require(SDL_ReadSurfacePixel(surface.get(), 5, 5, &r, &g, &b, &a) &&
                      g == 255 && r == 0,
                  "layout-only direction change must rebuild the layer raster");
    const auto arranged = root.stats().arranged;
    root.flushLayout(math::Size2{40, 20}, layout::LayoutDirection::RightToLeft);
    test::require(root.stats().arranged == arranged,
                  "raster invalidation must not keep layout dirty");
    test::require(layerPtr->estimatedCacheBytes() > 0,
                  "cache allocated under budget");
    layerPtr->dropCache();
    root.services().rasterBudget->limit = 0;
    render(layout::LayoutDirection::LeftToRight);
    test::require(
        layerPtr->estimatedCacheBytes() == 0 &&
            root.services().rasterBudget->used == 0,
        "root budget refusal renders uncached without reserving bytes");
    test::require(SDL_ReadSurfacePixel(surface.get(), 5, 5, &r, &g, &b, &a) &&
                      r == 255,
                  "uncached budget fallback preserves pixels");
    root.services().rasterBudget->limit = 64 * 1024 * 1024;
    layerPtr->applyPatch({.byteLimit = ui::Patch<std::size_t>::set(1)});
    render(layout::LayoutDirection::LeftToRight);
    test::require(layerPtr->estimatedCacheBytes() == 0,
                  "per-layer budget refusal is also uncached");
    layerPtr->applyPatch({.byteLimit = ui::Patch<std::size_t>::reset()});
    render(layout::LayoutDirection::LeftToRight);
    test::require(root.services().rasterBudget->used > 0,
                  "cache reservation restored when budget permits");
    root.setContent({});
    test::require(root.services().rasterBudget->used == 0,
                  "detach releases cache budget reservation");
  });
}
