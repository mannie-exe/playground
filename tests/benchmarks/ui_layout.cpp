#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string_view>

#include <app/Assets.hpp>
#include <app/SDLGuard.hpp>
#ifdef PLAYGROUND_WORKLOAD_GPU
#include "GPUPainter.hpp"
#include <platform/sdl/GPURenderBackend.hpp>
#include <support/GPUReadback.hpp>
#endif
#include <app/TTFGuard.hpp>
#include <demo2d/Demo2DUI.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/views/SettingsView.hpp>

using namespace playground;
using Clock = std::chrono::steady_clock;

namespace {
void invalidateTree(ui::Node &node) {
  for (const auto &child : node.children())
    invalidateTree(*child);
  node.invalidateLayout();
}

ui::ScrollView *findScroll(ui::Node &node) {
  if (auto *scroll = dynamic_cast<ui::ScrollView *>(&node);
      scroll &&
      scroll->contentExtent().height > scroll->viewportExtent().height)
    return scroll;
  for (const auto &child : node.children())
    if (auto *scroll = findScroll(*child))
      return scroll;
  return nullptr;
}

std::vector<unsigned char> pixels(ui::UIRoot &root, math::Size2 size) {
  SDLResource<SDL_Surface, SDL_DestroySurface> surface{SDL_CreateSurface(
      int(size.width), int(size.height), SDL_PIXELFORMAT_RGBA32)};
  test::require(bool(surface), "workload raster allocation");
  SDL_ClearSurface(surface.get(), 0, 0, 0, 0);
  root.prepare();
  sdl::SurfacePainter painter{*surface};
  root.render(painter);
  std::vector<unsigned char> result(std::size_t(size.width * size.height * 4));
  for (int y = 0; y < surface->h; ++y)
    std::memcpy(result.data() + y * surface->w * 4,
                static_cast<unsigned char *>(surface->pixels) +
                    y * surface->pitch,
                std::size_t(surface->w * 4));
  return result;
}

double pixelError(const std::vector<unsigned char> &a,
                  const std::vector<unsigned char> &b, bool gpu) {
  test::require(a.size() == b.size(), "reference pixel dimensions agree");
  if (!gpu)
    return a == b ? 0 : 1;
  double largest{};
  for (std::size_t i = 0; i < a.size(); i += sizeof(float)) {
    float x{}, y{};
    std::memcpy(&x, a.data() + i, sizeof(float));
    std::memcpy(&y, b.data() + i, sizeof(float));
    test::require(std::isfinite(x) && std::isfinite(y), "finite GPU readback");
    largest = std::max(largest, double(std::abs(x - y)));
  }
  return largest;
}

double elapsed(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}
} // namespace

int main(int argc, char **argv) {
  bool verify{}, full{}, gpu{};
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == "--gpu")
      gpu = true;
    else if (arg == "--verify")
      verify = true;
    else if (arg == "--full-layout")
      full = true;
    else {
      std::cerr
          << "Usage: ui_layout_workload [--verify] [--full-layout] [--gpu]\n";
      return 2;
    }
  }
  bool unsupported{};
  const auto result = test::run([&] {
    SDLGuard sdl{gpu ? SDL_INIT_VIDEO : 0};
    TTFGuard ttf;
#ifdef PLAYGROUND_WORKLOAD_GPU
    std::unique_ptr<sdl::gpu_detail::PaintDevice> device;
    if (gpu) {
      if (!sdl::packagedShaderFormats() ||
          !SDL_GPUSupportsShaderFormats(sdl::packagedShaderFormats(),
                                        "vulkan")) {
        unsupported = true;
        return;
      }
      device = std::make_unique<sdl::gpu_detail::PaintDevice>(
          std::make_shared<sdl::GPUDevice>(sdl::GPUDeviceProps{
              sdl::packagedShaderFormats(), false, "vulkan"}));
    }
#else
    if (gpu)
      throw std::runtime_error("GPU workload was not built");
#endif
    const auto raster = [&](ui::UIRoot &root, math::Size2 size) {
#ifdef PLAYGROUND_WORKLOAD_GPU
      if (device) {
        auto target =
            device->targets.color({int(size.width), int(size.height)});
        sdl::gpu_detail::GPUPainter painter{*device, {1, 1}};
        root.prepare({.images = painter.imagePreparer(),
                      .text = painter.textPreparer()});
        root.render(painter);
        painter.finish(target->get(), {int(size.width), int(size.height)}, {});
        const auto data = test::readPixels(device->device, *target->publish());
        const auto *begin =
            reinterpret_cast<const unsigned char *>(data.data());
        return std::vector<unsigned char>{begin, begin + data.size() *
                                                             sizeof(data[0])};
      }
#endif
      return pixels(root, size);
    };
    AssetRegistry assets;
    auto catalog = std::make_shared<assets::AssetCatalog>(
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets");
    app::registerAssets(*catalog);
    demo2d::registerAssets(*catalog);
    catalog->freeze();
    sdl::AssetResources resources{catalog, assets};
    auto font = resources.font(app::fontAsset, {.style = {.size = 18}});
    std::cout << "scene,width,repeat,full_layout,first_layout_ms,median_ms,p95_"
                 "ms,max_"
                 "ms,measured,text_layouts,arranged,preferred_preserves_offset,"
                 "pixels_equal,max_pixel_error\n";
    for (bool demo : {false, true})
      for (float width : {408.f, 900.f})
        for (int repeat = 0; repeat < 3; ++repeat) {
          ui::UIRoot root;
          auto theme = ui::defaultThemeDefinition();
          app::configureThemeFonts(theme.typography, resources);
          root.setThemeDefinition(theme);
          std::unique_ptr<ui::Node> content =
              demo ? demo2d::makeDemo2DUI(
                         assets, demo2d::acquireResources(resources, 18), {})
                   : ui::makeSettingsView(assets, font, {}, {});
          auto *tree = content.get();
          root.setContent(std::move(content));
          const math::Size2 size{width, 480};
          auto start = Clock::now();
          root.flushLayout(size);
          const auto cold = elapsed(start);
          auto *scroll = findScroll(*tree);
          test::require(scroll != nullptr,
                        "workload has real overflowing content");
          const auto limit =
              scroll->contentExtent().height - scroll->viewportExtent().height;
          scroll->setOffset({0, limit * .25f});
          root.flushLayout(size);
          for (int pass = 0; pass < 8 && root.needsUpdate(); ++pass)
            root.update(0);
          test::require(!root.needsUpdate(),
                        "workload layout settles before warm samples");
          const auto before = root.stats();
          std::vector<double> times;
          for (int step = 0; step < 12; ++step) {
            start = Clock::now();
            scroll->setOffset({0, limit * (step % 2 ? .25f : .75f)});
            if (full)
              invalidateTree(*tree);
            root.flushLayout(size);
            times.push_back(elapsed(start));
          }
          const auto delta = ui::workDelta(root.stats(), before);
          const auto optimized = raster(root, size);
          invalidateTree(*tree);
          root.flushLayout(size);
          const auto reference = raster(root, size);
          // RGBA16F blend rounding can differ by one half-float ULP after
          // rebuilding fractional layout/atlas geometry; software stays exact.
          const auto tolerance = gpu ? 1.0 / 1024 : 0.0;
          const auto error = pixelError(optimized, reference, gpu);
          const bool same = error <= tolerance;
          root.update(0);
          raster(root, size);
          root.update(0);
          test::require(
              !root.needsPaint(),
              "settled scene returns to idle without a redraw cooldown");
          const auto offset = scroll->offset();
          scroll->setOffset({0, limit * .5f});
          root.flushLayout(size);
          test::require(pixelError(optimized, raster(root, size), gpu) >
                            tolerance,
                        "pixel oracle detects stale scroll content");
          scroll->setOffset(offset);
          root.flushLayout(size);
          raster(root, size);
          const auto viewport = scroll->viewportExtent();
          const auto extent = scroll->contentExtent();
          root.preferredSize({1600, 1600});
          const bool preserved = scroll->offset() == offset &&
                                 scroll->viewportExtent() == viewport &&
                                 scroll->contentExtent() == extent;
          root.flushLayout(size);
          const bool roundTrip =
              pixelError(optimized, raster(root, size), gpu) <= tolerance;
          std::sort(times.begin(), times.end());
          std::cout << (demo ? "demo2d" : "settings") << ',' << width << ','
                    << repeat << ',' << full << ',' << cold << ','
                    << times[times.size() / 2] << ',' << times.back() << ','
                    << times.back() << ',' << delta.measured << ','
                    << delta.textLayouts << ',' << delta.arranged << ','
                    << preserved << ',' << same << ',' << error << '\n';
          if (verify) {
            test::require(same && roundTrip,
                          "retained scroll pixels equal forced full layout");
            test::require(
                preserved,
                "provisional measurement preserves live scroll geometry");
            test::require(full || delta.measured == 0,
                          "offset-only scrolling does not remeasure content");
          }
        }
  });
  return unsupported && result == 0 ? 77 : result;
}
