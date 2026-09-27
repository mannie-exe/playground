#include <platform/sdl/SurfacePainter.hpp>
#include <support/AssetRegistry.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Vector.hpp>

int main() {
  return playground::test::run([] {
    using namespace playground;
    AssetRegistry assets;
    auto doc = std::make_shared<const SVGDocument>(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20"><rect id="box" x="0" y="0" width="20" height="20" style="fill:#ff0000"/></svg>)");
    SVGStyleOverrides overrides{
        {"box", {.fill = math::ColorRGBA8{0, 255, 0, 255}}}};
    auto original = assets.getVector(VectorSource{doc}, {}, {20, 20});
    auto styled = assets.getVector(VectorSource{doc}, overrides, {20, 20});
    test::require(
        styled == assets.getVector(VectorSource{doc}, overrides, {20, 20}) &&
            original != styled,
        "styled variants cached independently");
    Uint8 r{}, g{}, b{}, a{};
    test::require(SDL_ReadSurfacePixel(styled.get(), 10, 10, &r, &g, &b, &a) &&
                      g == 255 && r == 0,
                  "override beats original inline fill");
    test::require(
        SDL_ReadSurfacePixel(original.get(), 10, 10, &r, &g, &b, &a) &&
            r == 255,
        "shared original unchanged");
    test::rejects([&] { doc->styled({{"missing", {.opacity = 0.5f}}}); },
                  "missing ID rejected");
    test::rejects([&] { doc->styled({{"box", {.strokeWidth = -1.0f}}}); },
                  "negative stroke rejected");
    auto invisible = assets.getVector(
        VectorSource{doc}, {{"box", {.fill = NoSVGPaint{}}}}, {20, 20});
    test::require(
        SDL_ReadSurfacePixel(invisible.get(), 10, 10, &r, &g, &b, &a) && a == 0,
        "explicit no-paint differs from absent override");
    auto translucent = assets.getVector(VectorSource{doc},
                                        {{"box", {.opacity = 0.5f}}}, {20, 20});
    SDL_ReadSurfacePixel(translucent.get(), 10, 10, &r, &g, &b, &a);
    test::require(a >= 126 && a <= 128, "element opacity affects raster alpha");
    test::rejects(
        [] {
          SVGDocument duplicate{R"(<svg><rect id="a"/><rect id="a"/></svg>)"};
          duplicate.styled({{"a", {.opacity = 1.0f}}});
        },
        "duplicate targeted IDs rejected");
    ui::UIRoot root;
    auto vector =
        std::make_unique<ui::Vector>(assets, ui::VectorProps{.source = doc});
    auto *node = vector.get();
    root.setContent(std::move(vector));
    root.flushLayout({20, 20});
    root.prepare();
    node->applyPatch(
        {.styles = playground::Patch<SVGStyleOverrides>::set(overrides)});
    root.flushLayout({20, 20});
    root.prepare();
    SurfaceHandle target{SDL_CreateSurface(20, 20, SDL_PIXELFORMAT_RGBA32),
                         SurfaceHandleDeleter{}};
    SDL_FillSurfaceRect(target.get(), nullptr, 0);
    sdl::SurfacePainter painter{*target};
    root.render(painter);
    SDL_ReadSurfacePixel(target.get(), 10, 10, &r, &g, &b, &a);
    test::require(g == 255 && r == 0,
                  "Vector style patch invalidates same-size raster");
    test::rejects(
        [&] {
          node->applyPatch({.styles = playground::Patch<SVGStyleOverrides>::set(
                                {{"missing", {}}})});
        },
        "failed override patch preserves props");
    test::require(node->props().styles == overrides,
                  "failed variant does not mutate live props");
  });
}
