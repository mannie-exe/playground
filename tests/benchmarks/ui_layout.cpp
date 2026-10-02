#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
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
#include <ui/containers/Box.hpp>
#include <ui/content/Vector.hpp>
#include <ui/controls/Meter.hpp>
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

void screenshots(const std::filesystem::path &directory, AssetRegistry &assets,
                 sdl::AssetResources &resources) {
  std::filesystem::create_directories(directory);
  for (const float width : {960.f, 408.f})
    for (const float scale : {1.f, 1.5f})
      for (const bool dark : {false, true}) {
        ui::UIRoot root;
        auto theme = ui::defaultThemeDefinition();
        app::configureThemeFonts(theme.typography, resources);
        theme.typography.textScale = scale;
        root.setThemeDefinition(theme);
        root.setAppearance(dark ? ui::ColorSchemePreference::Dark
                                : ui::ColorSchemePreference::Light,
                           ui::ContrastPreference::Normal, {});
        auto content = demo2d::makeDemo2DUI(
            assets,
            demo2d::acquireResources(resources, demo2d::config::textSize), {});
        auto *tree = content.get();
        root.setContent(std::move(content));
        const math::Size2 size{width, 760};
        root.flushLayout(size);
        auto *scroll = findScroll(*tree);
        test::require(scroll != nullptr, "gallery has overflowing controls");
        const float limit =
            scroll->contentExtent().height - scroll->viewportExtent().height;
        const int pages =
            int(std::ceil(limit / scroll->viewportExtent().height));
        for (int page = 0; page <= pages; ++page) {
          scroll->setOffset(
              {0, std::min(limit, page * scroll->viewportExtent().height)});
          root.flushLayout(size);
          auto data = pixels(root, size);
          SDLResource<SDL_Surface, SDL_DestroySurface> surface{
              SDL_CreateSurfaceFrom(int(width), 760, SDL_PIXELFORMAT_RGBA32,
                                    data.data(), int(width) * 4)};
          const auto path =
              directory /
              ("demo2d-" + std::to_string(int(width)) + "-" +
               std::to_string(int(scale * 100)) +
               (dark ? "-dark-" : "-light-") + std::to_string(page) + ".bmp");
          test::require(surface &&
                            SDL_SaveBMP(surface.get(), path.string().c_str()),
                        "save gallery screenshot");
          std::cout << path.string() << '\n';
        }
      }
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
  std::filesystem::path captureDirectory;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == "--gpu")
      gpu = true;
    else if (arg == "--screenshots" && i + 1 < argc)
      captureDirectory = argv[++i];
    else if (arg == "--verify")
      verify = true;
    else if (arg == "--full-layout")
      full = true;
    else {
      std::cerr
          << "Usage: ui_layout_workload [--verify] [--full-layout] [--gpu]\n"
          << "       ui_layout_workload --screenshots DIRECTORY (software)\n";
      return 2;
    }
  }
  if (!captureDirectory.empty() && (gpu || verify || full)) {
    std::cerr
        << "Screenshot mode uses software rendering without timing checks\n";
    return 2;
  }
  bool unsupported{};
  const auto result = test::run([&] {
    std::optional<SDLGuard> sdl;
    try {
      sdl.emplace(gpu ? SDL_INIT_VIDEO : 0);
    } catch (const std::runtime_error &error) {
      if (!gpu)
        throw;
      std::cerr << "Skipping GPU workload: " << error.what() << '\n';
      unsupported = true;
      return;
    }
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
        painter.finish(target->get(), {int(size.width), int(size.height)},
                       {0, 0, 0, 0});
        const auto data = test::readPixels(device->device, *target->publish());
        const auto *begin =
            reinterpret_cast<const unsigned char *>(data.data());
        return std::vector<unsigned char>{begin, begin + data.size() *
                                                             sizeof(data[0])};
      }
#endif
      return pixels(root, size);
    };
    // Compare animation samples to independently authored static scenes on
    // both software and GPU painters; include compositing and geometry.
    {
      ui::UIRoot animated, reference;
      auto moving = std::make_unique<ui::Box>();
      auto still = std::make_unique<ui::Box>();
      auto *a = moving.get();
      auto *b = still.get();
      a->setBackground(math::ColorRGBA8{240, 80, 30, 255});
      b->setBackground(math::ColorRGBA8{240, 80, 30, 255});
      animated.setContent(std::move(moving));
      reference.setContent(std::move(still));
      animated.flushLayout({32, 32});
      reference.flushLayout({32, 32});
      auto fade = animated.motion().transition(ui::motion::opacity(a->handle()),
                                               0.f, {.duration = 1});
      auto move =
          animated.motion().transition(ui::motion::translation(a->handle()),
                                       math::Vec2f{16, 0}, {.duration = 1});
      auto scale =
          animated.motion().transition(ui::motion::scale(a->handle()),
                                       math::Vec2f{.7f, 1.1f}, {.duration = 1});
      auto rotate = animated.motion().transition(
          ui::motion::rotation(a->handle()), .25f, {.duration = 1});
      const auto measured = animated.stats().measured;
      for (int step = 0; step <= 4; ++step) {
        if (step)
          animated.update(.25);
        animated.motion().sample();
        b->setMotionValue(ui::MotionProperty::Opacity, 1.f - step * .25f);
        b->setMotionValue(ui::MotionProperty::Translation,
                          math::Vec2f{step * 4.f, 0});
        b->setMotionValue(ui::MotionProperty::Scale,
                          math::Vec2f{1.f - step * .075f, 1.f + step * .025f});
        b->setMotionValue(ui::MotionProperty::Rotation, step * .0625f);
        const auto actual = raster(animated, {32, 32});
        const auto expected = raster(reference, {32, 32});
        test::require(
            pixelError(actual, expected, gpu) <= (gpu ? .002 : 0),
            "animated opacity/translation match static reference pixels");
      }
      test::require(animated.stats().measured == measured,
                    "presentation animation does not remeasure layout");
      animated.update(0);
      test::require(!animated.motion().needsFrame() &&
                        !animated.motion().nextDelay() &&
                        !animated.needsPaint(),
                    "finished motion returns to idle after final render");
    }
    AssetRegistry assets;
    auto catalog = std::make_shared<assets::AssetCatalog>(
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets");
    app::registerAssets(*catalog);
    demo2d::registerAssets(*catalog);
    catalog->freeze();
    sdl::AssetResources resources{catalog, assets};
    if (!captureDirectory.empty()) {
      screenshots(captureDirectory, assets, resources);
      return;
    }
    // A split overlay must equal the corresponding parts of two single-ink
    // references, including the icon's transparent exclamation cutout.
    for (unsigned appearance = 0; appearance < 5; ++appearance) {
      const bool dark = appearance & 1, high = appearance & 2;
      ui::UIRoot root;
      if (appearance == 4) {
        auto theme = ui::defaultThemeDefinition();
        theme.light.meterTrack = {255, 255, 255, 255};
        theme.light.warning = {0, 0, 0, 255};
        theme.light.error = {0, 0, 80, 255};
        theme.light.meterOnTrack = {0, 0, 0, 255};
        theme.light.onWarning = {255, 255, 255, 255};
        theme.light.onError = {255, 220, 80, 255};
        root.setThemeDefinition(theme);
      }
      root.setAppearance(dark ? ui::ColorSchemePreference::Dark
                              : ui::ColorSchemePreference::Light,
                         high ? ui::ContrastPreference::High
                              : ui::ContrastPreference::Normal,
                         {});
      const auto icon = [&](auto id) {
        return std::make_unique<ui::Vector>(
            assets,
            ui::VectorProps{.source = resources.vector(id),
                            .colorTreatment = ui::ColorTreatment::Adaptive});
      };
      auto meter = std::make_unique<ui::Meter>(icon(demo2d::warningIconAsset),
                                               icon(demo2d::errorIconAsset));
      auto *control = meter.get();
      root.setContent(std::move(meter));
      const auto sample = [&](std::optional<double> value, bool critical) {
        control->setProps(
            {.value = value,
             .warning = 0,
             .critical = critical ? std::optional<double>{0} : std::nullopt});
        root.flushLayout({200, 16});
        return raster(root, {200, 16});
      };
      const auto warningShape = sample(0, false);
      const auto criticalShape = sample(0, true);
      test::require(pixelError(warningShape, criticalShape, gpu) > .0001,
                    "warning and critical SVGs have distinct visible shapes");
      const auto stride = gpu ? 4 * sizeof(float) : 4;
      for (const bool critical : {false, true}) {
        const auto empty = sample(0, critical);
        const auto full = sample(100, critical);
        if (high)
          test::require(std::memcmp(empty.data() + (8 * 200 + 199) * stride,
                                    empty.data() + (8 * 200 + 100) * stride,
                                    stride) != 0 &&
                            std::memcmp(empty.data() + 100 * stride,
                                        empty.data() + (8 * 200 + 199) * stride,
                                        stride) == 0,
                        "high contrast outlines the empty range at its top and "
                        "right edges");
        for (unsigned y = 0; y < 16; ++y)
          for (unsigned x = 192; x < (high ? 198u : 200u); ++x)
            test::require(
                std::memcmp(full.data() + (y * 200 + x) * stride,
                            full.data() + (y * 200 + 100) * stride,
                            stride) == 0,
                "status icon leaves eight units of filled track to its right");
        for (const double value : {75., 90., 92., 95.}) {
          const auto actual = sample(value, critical);
          auto expected = empty;
          const auto filledWidth = std::size_t(value * 2);
          for (unsigned y = 0; y < 16; ++y)
            std::memcpy(expected.data() + y * 200 * stride,
                        full.data() + y * 200 * stride, filledWidth * stride);
          test::require(
              pixelError(actual, expected, gpu) <= (gpu ? .002 : 0),
              "meter overlay clips paired inks exactly at the fill boundary");
        }
        const auto over = sample(120, critical);
        // Over-limit outline may change outer pixels, but not the marker.
        for (unsigned y = 2; y < 14; ++y)
          test::require(std::memcmp(over.data() + (y * 200 + 176) * stride,
                                    full.data() + (y * 200 + 176) * stride,
                                    16 * stride) == 0,
                        "over-limit meter retains the filled indicator ink");
      }
      const auto unavailable = sample({}, false);
      for (unsigned y = 2; y < 14; ++y)
        for (unsigned x = 176; x < 192; ++x)
          test::require(
              std::memcmp(unavailable.data() + (y * 200 + x) * stride,
                          unavailable.data() + (y * 200 + 100) * stride,
                          stride) == 0,
              "unavailable meter has no status icon");
      control->setProps({.value = 20, .warning = 70, .critical = 90});
      root.flushLayout({200, 16});
      const auto normal = raster(root, {200, 16});
      for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 176; x < (high ? 198u : 200u); ++x)
          test::require(std::memcmp(normal.data() + (y * 200 + x) * stride,
                                    normal.data() + (y * 200 + 100) * stride,
                                    stride) == 0,
                        "normal meter keeps a full-width track with no marker");
    }
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
