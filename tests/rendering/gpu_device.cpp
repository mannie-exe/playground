#include <cmath>
#include <filesystem>
#include <thread>
#include <type_traits>

#include <SDL3/SDL_log.h>

#include "GPUPainter.hpp"
#include "GPUSceneRenderer.hpp"
#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/RenderBackendFactory.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <support/AssetRegistry.hpp>
#include <support/GPUReadback.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Text.hpp>

using namespace playground;
using namespace playground::sdl;
using namespace playground::sdl::gpu_detail;

static_assert(!std::is_constructible_v<GPUImage, GPUDeviceHandle, math::Vec2i>);

using test::readPixel;
using test::readPixels;

static void verifyPaintFastPath(PaintDevice &paint) {
  const auto render = [&](bool fast) {
    paint.rectangularFastPath = fast;
    auto target = paint.targets.color({32, 32});
    GPUPainter p{paint, {1, 1}};
    p.clip(math::rect(1.25f, 2.5f, 28.5f, 26.25f));
    p.fill(math::rect(.25f, .5f, 20.5f, 24.25f), {120, 80, 220, 200});
    p.save();
    p.translate({24, 10});
    p.transform(math::Transform2D::scaling({-1, 1}));
    p.fill(math::rect(0, 0, 8, 8), {20, 255, 120, 170});
    p.restore();
    p.beginLayer(math::rect(5, 6, 19, 20), .6f);
    p.fill(math::rect(6.5f, 7.25f, 12.5f, 14.5f), {255, 0, 0, 200});
    p.endLayer();
    p.save();
    p.translate({10, 9});
    p.transform(math::Transform2D::rotation(.2f));
    p.paintRoundedBox({math::rect(0, 0, 18, 12), math::CornerRadii::all(3)},
                      math::Insets::all(2), {20, 100, 200, 180},
                      math::ColorRGBA8{255, 255, 0, 255});
    p.restore();
    p.finish(target->get(), {32, 32}, {0, 0, 0, 0});
    return readPixels(paint.device, *target->publish());
  };
  const auto fast = render(true), general = render(false);
  for (std::size_t i = 0; i < fast.size(); ++i)
    for (int c = 0; c < 4; ++c)
      test::require(std::abs(fast[i][c] - general[i][c]) < .015f,
                    "rectangle specialization preserves mirrored transforms, "
                    "fractional clips, rebased layers and rounded borders");
  paint.rectangularFastPath = true;
  const auto before = paint.stats();
  GPUPainter p{paint, {1, 1}};
  p.fill(math::rect(0, 0, 5, 5), {255, 0, 0, 255});
  p.clip(math::rect(10, 10, 2, 2));
  p.fill(math::rect(0, 0, 5, 5), {0, 255, 0, 255});
  math::Path2D path;
  path.moveTo({0, 0}).lineTo({4, 0}).lineTo({4, 4}).close();
  p.drawPath(path, {.fill = math::ColorRGBA8{0, 0, 255, 255}});
  auto target = paint.targets.color({32, 32});
  p.finish(target->get(), {32, 32}, {0, 0, 0, 255});
  test::require(paint.stats().rejected == before.rejected + 2 &&
                    readPixel(paint.device, *target, 2, 2)[0] > .99f,
                "empty clip intersection rejects shapes and paths without "
                "modifying earlier draw");
}

static void verifyStyledAtlas(PaintDevice &paint, AssetRegistry &assets) {
  FontProps props{.path = std::string{PLAYGROUND_SOURCE_DIR} +
                          "/assets/fonts/LBRITE.TTF",
                  .style = {.size = 24},
                  .layout = {.lineSpace = 32}};
  bool allPassed = true;
  const auto verify = [&](const FontProps &fontProps, std::string_view value,
                          int wrap) {
    auto font = assets.getFont(fontProps);
    constexpr SDL_Color tint{64, 180, 230, 192};
    SDLResource<SDL_Surface, SDL_DestroySurface> cpu{
        TTF_RenderText_Blended_Wrapped(font->get(), value.data(), value.size(),
                                       tint, wrap)};
    test::require(bool(cpu), "styled CPU text fixture renders");
    auto gpu = std::dynamic_pointer_cast<const GPUImage>(
        paint.images.prepareText(FontTextSource{
            font, std::string{value}, wrap, {tint.r, tint.g, tint.b, tint.a}}));
    test::require(paint.images.lastTextPreparation() == TextPreparation::Atlas,
                  "patched styled text uses actual atlas geometry");
    test::require(gpu->pixelSize() == math::Size2{float(cpu->w), float(cpu->h)},
                  "styled atlas and CPU extent agree");
    const auto pixels = readPixels(paint.device, *gpu);
    double error{}, expectedInk{};
    for (int y = 0; y < cpu->h; ++y)
      for (int x = 0; x < cpu->w; ++x) {
        Uint8 r{}, g{}, b{}, a{};
        test::require(SDL_ReadSurfacePixel(cpu.get(), x, y, &r, &g, &b, &a),
                      "CPU styled pixel");
        const auto actual = pixels[std::size_t(y * cpu->w + x)];
        expectedInk += a / 255.f;
        error += std::abs(actual[3] - a / 255.f);
      }
    // SDL's CPU glyph-overlap arithmetic differs slightly from alpha-over.
    if (error > std::max(1., expectedInk * .03))
      std::cerr << "Styled atlas flags=" << fontProps.style.flags
                << " outline=" << fontProps.style.outline << " wrap=" << wrap
                << " ink=" << expectedInk << " absolute error=" << error
                << '\n';
    allPassed = allPassed && expectedInk > 0 &&
                error <= std::max(1., expectedInk * .03);
  };
  verify(props, "A K", 0);
  props.style.flags = TTF_STYLE_BOLD;
  verify(props, "A K", 0);
  props.style.flags = TTF_STYLE_ITALIC;
  verify(props, "A K", 0);
  props.style.flags = TTF_STYLE_NORMAL;
  props.style.outline = 1;
  verify(props, "A K", 0);
  props.style.flags = TTF_STYLE_BOLD | TTF_STYLE_ITALIC;
  props.style.outline = 1;
  verify(props, "A K", 0);
  props.style.outline = 0;
  props.style.flags = TTF_STYLE_UNDERLINE | TTF_STYLE_STRIKETHROUGH;
  verify(props, "A K", 0);
  props.style.flags = TTF_STYLE_NORMAL;
  props.layout.alignment = TTF_HORIZONTAL_ALIGN_CENTER;
  verify(props, "A K A K A K", 60);
  props.layout.direction = TTF_DIRECTION_RTL;
  verify(props, "A K", 0);
  auto font = assets.getFont(props);
  if (TTF_FontHasGlyph(font->get(), 0x0301))
    verify(props, "A\xCC\x81 K", 0);
  test::require(
      allPassed,
      "styled atlas preserves glyph/decorations/wrapped geometry and alpha");
}

#ifdef PLAYGROUND_COLOR_FONT
static void compareColorText(PaintDevice &paint, const FontHandle &font,
                             const std::string &value, int wrap,
                             math::ColorRGBA8 foreground) {
  SDLResource<SDL_Surface, SDL_DestroySurface> cpu{
      TTF_RenderText_Blended_Wrapped(
          font->get(), value.data(), value.size(),
          {foreground.r, foreground.g, foreground.b, foreground.a}, wrap)};
  test::require(bool(cpu), "CPU mixed/color text raster");
  auto gpu = std::dynamic_pointer_cast<const GPUImage>(
      paint.images.prepareText(FontTextSource{font, value, wrap, foreground}));
  test::require(
      paint.images.lastTextPreparation() == TextPreparation::Atlas &&
          gpu->pixelSize() == math::Size2{float(cpu->w), float(cpu->h)},
      "mixed/color text uses native atlas with matching measured extent");
  const auto pixels = readPixels(paint.device, *gpu);
  int opaque{}, partial{};
  float maximumError{};
  for (int y = 0; y < cpu->h; ++y)
    for (int x = 0; x < cpu->w; ++x) {
      math::ColorRGBA8 pixel{};
      test::require(SDL_ReadSurfacePixel(cpu.get(), x, y, &pixel.r, &pixel.g,
                                         &pixel.b, &pixel.a),
                    "read straight-alpha CPU text pixel");
      const auto linear = math::premultiply(math::toLinear(pixel));
      const std::array<float, 4> expected{linear.r, linear.g, linear.b,
                                          linear.a};
      for (int channel = 0; channel < 4; ++channel)
        maximumError =
            std::max(maximumError,
                     std::abs(pixels[std::size_t(y * cpu->w + x)][channel] -
                              expected[channel]));
      opaque += pixel.a == foreground.a && pixel.a > 0;
      partial += pixel.a > 0 && pixel.a < foreground.a;
    }
  if (maximumError > .02f)
    std::cerr << "Mixed/color text pixel error=" << maximumError
              << " wrap=" << wrap << '\n';
  test::require(
      opaque > 0 && partial > 0 && maximumError < .02f,
      "complete mixed/color atlas output matches CPU geometry, tint and alpha");
}

static void verifyColoredAtlas(PaintDevice &paint, AssetRegistry &assets,
                               const std::string &path, Uint32 codepoint,
                               const std::string &value,
                               TextPreparation expectedPreparation) {
  auto font = assets.getFont(FontProps{
      .path = path, .style = {.size = 48}, .layout = {.lineSpace = 64}});
  TTF_ImageType type{};
  SDLResource<SDL_Surface, SDL_DestroySurface> glyph{
      TTF_GetGlyphImage(font->get(), codepoint, &type)};
  test::require(glyph && type == TTF_IMAGE_COLOR,
                "actual color glyph, not missing-glyph fallback");
  auto engine = std::make_shared<GPUTextEngine>(paint.device);
  GPUText native{engine, font, value};
  const auto *sequence = native.drawData();
  test::require(
      sequence && sequence->image_type == TTF_IMAGE_COLOR,
      "layer-only and outline-base color fonts produce native atlas quads");
  test::require(expectedPreparation == TextPreparation::Atlas,
                "all tested color fonts use atlas");
  compareColorText(paint, font, value, 0, {255, 255, 255, 255});
  compareColorText(paint, font, value, 0, {0, 255, 0, 128});
}

static void verifyMixedText(PaintDevice &paint, AssetRegistry &assets) {
  auto base = assets.getFont(FontProps{
      .path = std::string{PLAYGROUND_SOURCE_DIR} + "/assets/fonts/LBRITE.TTF",
      .style = {.size = 48},
      .layout = {.lineSpace = 64}});
  test::require(!TTF_FontHasGlyph(base->get(), 0x3297),
                "mixed fixture exercises the fallback font");
  auto normal = assets.getFont(FontProps{
      .path = std::string{PLAYGROUND_SOURCE_DIR} + "/assets/fonts/LBRITE.TTF",
      .style = {.size = 48},
      .layout = {.lineSpace = 64},
      .fallbacks = {{PLAYGROUND_COLOR_FONT, {}}}});
  test::require(TTF_FontHasGlyph(normal->get(), 0x3297),
                "registered fallback supplies the missing glyph");

  const std::string value{"A  \xE3\x8A\x97  K"};
  auto engine = std::make_shared<GPUTextEngine>(paint.device);
  GPUText native{engine, normal, value};
  bool mono{}, color{};
  for (auto *s = native.drawData(); s; s = s->next) {
    mono |= s->image_type == TTF_IMAGE_ALPHA;
    color |= s->image_type == TTF_IMAGE_COLOR;
  }
  test::require(
      mono && color,
      "one shaped text contains both monochrome and color atlas runs");
  compareColorText(paint, normal, value, 0, {64, 180, 230, 192});
  compareColorText(paint, normal, value + " " + value, 160,
                   {64, 180, 230, 192});
}
#endif

int main(int argc, char **argv) {
  try {
    SDLGuard sdl{SDL_INIT_VIDEO};
    TTFGuard ttf;
    const char *driver = argc > 1 ? argv[1] : "vulkan";
    if (!sdl::packagedShaderFormats() ||
        !SDL_GPUSupportsShaderFormats(sdl::packagedShaderFormats(), driver))
      return 77;
    return test::run([&] {
      auto device = std::make_shared<sdl::GPUDevice>(
          sdl::GPUDeviceProps{sdl::packagedShaderFormats(), true, driver});
      test::require(device->resourceDomain() !=
                        rendering::ResourceDomainId::cpu(),
                    "GPU and CPU realization domains differ");
      test::rejects<std::invalid_argument>(
          [&] { device->acquireCommands(""); },
          "invalid labels are rejected even with GPU profiling disabled");
      bool rejectedThread{};
      std::thread foreign{[&] {
        try {
          device->checkOwnerThread();
        } catch (const std::logic_error &) {
          rejectedThread = true;
        }
      }};
      foreign.join();
      test::require(rejectedThread,
                    "owner-thread GPU boundary rejects foreign work");
      {
        const auto before = device->resources()->snapshot();
        rendering::RenderRuntime runtime{device->resources()};
        runtime.attachDomain(device->resourceDomain());
        auto frame = runtime.beginFrame(rendering::RenderClock::now(),
                                        device->resourceDomain(), 1);
        device->beginFrame(frame);
        rendering::RGBA8Image pixels{.size = {8, 8},
                                     .pixels = std::vector<std::uint8_t>(256)};
        auto uploaded = std::make_shared<GPUImage>(device, pixels);
        device->endFrame();
        frame.reset();
        uploaded.reset();
        const auto pending = device->resources()->snapshot();
        test::require(
            runtime.snapshot().outstandingFrames == 1 &&
                pending.memory[1].bytes == before.memory[1].bytes + 256 &&
                pending.memory[0].bytes == before.memory[0].bytes + 256,
            "abandoned upload retains frame, texture and staging until "
            "observed completion");
        test::require(SDL_WaitForGPUIdle(device->get()),
                      "upload lifetime completion");
        device->pollCompletions();
        const auto retired = device->resources()->snapshot();
        test::require(
            runtime.snapshot().outstandingFrames == 0 &&
                retired.memory[1].bytes == before.memory[1].bytes &&
                retired.memory[0].bytes == before.memory[0].bytes,
            "completion releases texture/staging charges and frame credit");
        frame = runtime.beginFrame(rendering::RenderClock::now(),
                                   device->resourceDomain(), 2);
        device->beginFrame(frame);
        auto *commands = device->acquireCommands("canceled frame");
        device->endFrame();
        frame.reset();
        test::require(runtime.snapshot().outstandingFrames == 1,
                      "unsubmitted recording retains its admission credit");
        device->cancel(commands);
        test::require(runtime.snapshot().outstandingFrames == 0,
                      "successful cancellation retires recording credit");
      }
      PaintDevice paint{device};
      {
        TargetPool pool{device};
        auto writable = pool.color({4, 4});
        test::rejects<std::logic_error>([&] { writable->publish(); },
                                        "uninitialized targets are not images");
        GPUPainter initialize{paint, {1, 1}};
        initialize.finish(writable->get(), {4, 4}, {});
        auto published = writable->publish();
        test::require(SDL_WaitForGPUIdle(device->get()),
                      "publication completion");
        device->pollCompletions();
        auto *identity = writable.get();
        writable.reset();
        auto other = pool.color({4, 4});
        test::require(other.get() != identity,
                      "published image ownership prevents target recycling");
        published.reset();
        auto reused = pool.color({4, 4});
        test::require(reused.get() == identity,
                      "released publication permits reuse");
        test::rejects<std::logic_error>(
            [&] { reused->publish(); },
            "reuse requires a fresh initializing submission");
      }
      {
        GPUCommandAPI failedSubmission;
        failedSubmission.submit =
            +[](SDL_GPUCommandBuffer *commands) -> SDL_GPUFence * {
          SDL_CancelGPUCommandBuffer(commands);
          SDL_SetError("injected submit failure");
          return nullptr;
        };
        auto failing = std::make_shared<GPUDevice>(
            GPUDeviceProps{packagedShaderFormats(), true, driver},
            failedSubmission);
        TargetPool pool{failing};
        auto held = pool.color({4, 4});
        Commands recording{failing};
        failing->recordTexture(recording.value, held->get());
        bool classified{};
        try {
          recording.submit();
        } catch (const rendering::RenderFailure &error) {
          classified = error.operation() == rendering::RenderOperation::Submit;
        }
        test::require(
            classified && failing->pendingSubmissions() == 1 &&
                held->isLeased(),
            "ambiguous submit retains leases and classifies recovery");
        test::rejects<rendering::RenderFailure>([&] { pool.color({4, 4}); },
                                                "failed domain forbids reuse");
        test::rejects<rendering::RenderFailure>(
            [&] { held->publish(); }, "failed domain forbids publication");
        failing->invalidate();
        test::require(
            held->isLeased(),
            "invalidation quarantines unknown work until confirmed retirement");
        test::require(failing->retireInvalidatedSubmissions() &&
                          !held->isLeased(),
                      "explicit successful idle confirmation retires "
                      "quarantined submissions");
      }
      {
        device->setProfilingEnabled(true);
        SDL_Log("Native Vulkan timestamp support: %s",
                device->supportsTimestamps() ? "yes" : "no");
        PaintDevice timedPaint{device};
        auto timedTarget = timedPaint.targets.color({4, 4});
        GPUPainter timed{timedPaint, {1, 1}};
        timed.fill(math::rect(0, 0, 4, 4), {255, 0, 0, 255});
        timed.finish(timedTarget->get(), {4, 4}, {});
        test::require(readPixel(device, *timedTarget, 1, 1)[0] > .99f,
                      "timed commands still render correctly");
        const auto samples = device->takeGPUTimings();
        if (device->supportsTimestamps()) {
          test::require(
              !samples.empty(),
              "native timestamp samples arrive after fence completion");
          for (const auto &sample : samples)
            test::require(
                sample.sequence > 0 &&
                    sample.domain == device->resourceDomain() &&
                    std::isfinite(sample.milliseconds) &&
                    sample.milliseconds >= 0 &&
                    sample.completionLatencyMilliseconds.has_value() &&
                    *sample.completionLatencyMilliseconds >= 0,
                "native timestamps carry duration, domain and completion "
                "latency");
        } else {
          test::require(samples.empty(),
                        "unsupported timing is absent, not zero");
        }
        device->setProfilingEnabled(false);
      }
      {
        TargetPool pool{device};
        auto first = pool.color({4, 4});
        auto second = pool.color({4, 4});
        test::require(first != second && pool.stats().reuses == 0,
                      "live immutable handles are never recycled");
        auto *identity = first.get();
        first.reset();
        auto reused = pool.color({4, 4});
        test::require(reused.get() == identity && pool.stats().reuses == 1,
                      "unleased target reuse never waits for device idle");
        reused.reset();
        {
          Commands recording{device};
          auto held = pool.color({8, 8});
          const auto heldIdentity = held.get();
          device->recordTexture(recording.value, held->get());
          held.reset();
          auto distinct = pool.color({8, 8});
          test::require(
              distinct.get() != heldIdentity && pool.stats().busyMisses > 0,
              "unsubmitted recording leases prevent target overwrite");
          test::require(pool.liveBytes() >= 2 * 8 * 8 * 8,
                        "live accounting includes recording targets");
        }
        const auto reuseCount = pool.stats().reuses;
        auto afterCancel = pool.color({8, 8});
        test::require(pool.stats().reuses == reuseCount + 1,
                      "cancellation releases recording lease for reuse");
        afterCancel.reset();
        second.reset();
        pool.trim();
        test::require(pool.stats().retainedBytes == 0,
                      "pool trim drops unused targets");
      }
      auto target = paint.targets.color({32, 32});
      GPUPainter painter{paint, {1, 1}};
      painter.fill(math::rect(0, 0, 32, 32), {0, 0, 255, 255});
      painter.save();
      painter.clipRounded(
          {math::rect(4, 4, 24, 24), math::CornerRadii::all(6)});
      painter.fill(math::rect(0, 0, 32, 32), {255, 0, 0, 255});
      painter.restore();
      painter.finish(target->get(), {32, 32}, {0, 0, 0, 255});
      test::require(paint.stats().quads == 2 && paint.stats().drawCalls == 2 &&
                        paint.stats().rectangularQuads == 1 &&
                        paint.stats().generalQuads == 1,
                    "rounded clips split general and rectangular batches "
                    "without reordering");
      auto center = readPixel(device, *target, 16, 16),
           corner = readPixel(device, *target, 0, 0);
      test::require(center[0] > .99f && center[2] < .01f && corner[2] > .99f,
                    "GPU shapes and clip pixels");
      verifyPaintFastPath(paint);
      GPUPainter subpixel{paint, {1, 1}};
      subpixel.fill(math::rect(.75f, 0, 8, 8), {255, 0, 0, 255});
      subpixel.finish(target->get(), {32, 32}, {0, 0, 0, 255});
      test::require(std::abs(readPixel(device, *target, 0, 4)[0] - .25f) < .01f,
                    "fractional outer edges include partial pixel coverage");
      {
        math::Path2D ring;
        ring.moveTo({0, 0})
            .lineTo({24, 0})
            .lineTo({24, 24})
            .lineTo({0, 24})
            .close();
        ring.moveTo({8, 8})
            .lineTo({16, 8})
            .lineTo({16, 16})
            .lineTo({8, 16})
            .close();
        GPUPainter paths{paint, {1, 1}};
        paths.translate({4, 4});
        paths.clip(math::rect(0, 0, 20, 24));
        paths.drawPath(ring, {.fill = math::ColorRGBA8{255, 0, 0, 255},
                              .fillRule = math::FillRule::EvenOdd});
        math::Path2D line;
        line.moveTo({0, 12}).lineTo({20, 12});
        paths.drawPath(line, {.fill = std::nullopt,
                              .stroke = math::ColorRGBA8{0, 255, 0, 255},
                              .strokeWidth = 2});
        paths.finish(target->get(), {32, 32}, {0, 0, 0, 255});
        test::require(readPixel(device, *target, 6, 6)[0] > .99f &&
                          readPixel(device, *target, 15, 14)[0] < .01f &&
                          readPixel(device, *target, 15, 16)[1] > .99f &&
                          readPixel(device, *target, 26, 6)[0] < .01f,
                      "direct GPU path hole, stroke, transform and clip");
        math::Path2D oversized;
        oversized.moveTo({0, 0});
        for (int i = 1; i <= 256; ++i)
          oversized.lineTo({float(i), float(i % 2)});
        test::rejects<std::length_error>(
            [&] { paths.drawPath(oversized, {}); },
            "path draw rejects uniform-capacity overflow");
      }
      GPUPainter layers{paint, {1, 1}};
      layers.beginLayer(math::rect(0, 0, 32, 32), .5f);
      layers.fill(math::rect(0, 0, 32, 32), {255, 0, 0, 255});
      layers.fill(math::rect(0, 0, 16, 32), {255, 0, 0, 255});
      layers.endLayer();
      layers.finish(target->get(), {32, 32}, {0, 0, 0, 255});
      test::require(std::abs(readPixel(device, *target, 8, 8)[0] - .5f) < .01f,
                    "group opacity is applied once");
      GPUPainter recovered{paint, {1, 1}};
      recovered.fill(math::rect(0, 0, 32, 32), {0, 0, 255, 255});
      test::rejects<std::runtime_error>(
          [&] {
            rendering::LayerScope scope{recovered, math::rect(0, 0, 32, 32),
                                        .5f};
            recovered.fill(math::rect(0, 0, 32, 32), {255, 0, 0, 255});
            throw std::runtime_error("callback failed");
          },
          "layer callback failure propagates");
      test::rejects<std::runtime_error>(
          [&] {
            recovered.capture(math::rect(0, 0, 8, 8), {1, 1},
                              [](rendering::PaintContext &) {
                                throw std::runtime_error("capture failed");
                              });
          },
          "capture failure propagates");
      recovered.finish(target->get(), {32, 32}, {0, 0, 0, 255});
      test::require(readPixel(device, *target, 16, 16)[2] > .99f,
                    "failed layers/captures preserve parent recording");
      auto captured = recovered.capture(
          math::rect(4, 4, 8, 8), {2, 2}, [](rendering::PaintContext &p) {
            p.fill(math::rect(4, 4, 8, 8), {0, 255, 0, 255});
          });
      const auto capturedGPU =
          std::dynamic_pointer_cast<const GPUImage>(captured);
      test::require(captured->pixelSize() == math::Size2{16, 16} &&
                        readPixel(device, *capturedGPU, 8, 8)[1] > .99f,
                    "capture maps origin and density");
      GPUSceneRenderer scene{paint};
      test::require(scene.resourceDomain() == device->resourceDomain(),
                    "direct GPU scene service identifies its device domain");
      auto mesh = scene::makeMesh(scene::MeshData{
          {{{-.8f, -.8f, .5f}}, {{.8f, -.8f, .5f}}, {{0, .8f, .5f}}},
          {0, 1, 2}});
      const std::array draws{scene::MeshDraw{mesh, {{0, 255, 0, 255}}, {}}};
      auto rendered = std::dynamic_pointer_cast<const sdl::GPUImage>(
          scene.render({{}, {32, 32}, {0, 0, 0, 255}}, draws));
      test::require(readPixel(device, *rendered, 16, 16)[1] > .99f,
                    "GPU scene pixels");
      {
        auto front = scene::makeMesh(scene::MeshData{
            {{{-.8f, -.8f, .5f}}, {{.8f, -.8f, .5f}}, {{0, .8f, .5f}}},
            {0, 2, 1}});
        auto frontDraw = scene::MeshDraw{front, {{0, 255, 0, 255}}, {}};
        frontDraw.material.doubleSided = false;
        auto image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&frontDraw, 1}));
        test::require(readPixel(device, *image, 16, 16)[1] > .99f,
                      "LH glTF-facing triangle survives backface culling");
        frontDraw.mesh = mesh;
        image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&frontDraw, 1}));
        test::require(readPixel(device, *image, 16, 16)[1] < .01f,
                      "reverse triangle is culled");
        frontDraw.mesh = front;
        frontDraw.model.at(0, 0) = -1;
        image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&frontDraw, 1}));
        test::require(readPixel(device, *image, 16, 16)[1] > .99f,
                      "mirrored model preserves declared front face");
      }
      {
        auto samples = std::make_shared<GPUImage>(
            device,
            rendering::RGBA8Image{
                .size = {2, 1}, .pixels = {0, 0, 0, 255, 255, 255, 255, 255}});
        auto constantUV = scene::MeshData{{{{-.8f, -.8f, .5f}, {}, {.5f, .5f}},
                                           {{.8f, -.8f, .5f}, {}, {.5f, .5f}},
                                           {{0, .8f, .5f}, {}, {.5f, .5f}}},
                                          {0, 1, 2}};
        scene::MeshDraw textured{scene::makeMesh(constantUV),
                                 {.baseColorImage = samples,
                                  .sampling = rendering::Sampling::Linear},
                                 {}};
        auto image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&textured, 1}));
        test::require(
            std::abs(readPixel(device, *image, 16, 16)[0] - .5f) < .01f,
            "3D bilinear sampling interpolates decoded linear texels");
        for (auto &vertex : constantUV.vertices)
          vertex.uv.x = 1.25f;
        textured.mesh = scene::makeMesh(std::move(constantUV));
        textured.material.sampling = rendering::Sampling::Nearest;
        textured.material.addressU = scene::TextureAddress::Repeat;
        image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&textured, 1}));
        test::require(readPixel(device, *image, 16, 16)[0] < .01f,
                      "3D repeat addressing");
        textured.material.addressU = scene::TextureAddress::MirroredRepeat;
        image = std::dynamic_pointer_cast<const GPUImage>(scene.render(
            {{}, {32, 32}, {0, 0, 0, 255}}, std::span{&textured, 1}));
        test::require(readPixel(device, *image, 16, 16)[0] > .99f,
                      "3D mirrored addressing");
      }
      AssetRegistry assets;
      verifyStyledAtlas(paint, assets);
      auto font =
          assets.getFont(FontProps{.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                           "/assets/fonts/LBRITE.TTF"});
      auto text = paint.images.prepareText(
          FontTextSource{font, "I", 0, {255, 255, 255, 255}});
      test::require(text->pixelSize().width > 0, "GPU atlas produces an image");
      const auto atlas = std::dynamic_pointer_cast<const sdl::GPUImage>(text);
      const auto ink =
          readPixel(device, *atlas, int(text->pixelSize().width / 2),
                    int(text->pixelSize().height / 2));
      test::require(
          ink[3] > .1f,
          "atlas contains glyph coverage, not merely a sized empty image");
#ifdef PLAYGROUND_COLOR_FONT
      verifyColoredAtlas(paint, assets, PLAYGROUND_COLOR_FONT, 0x3297,
                         "\xE3\x8A\x97", TextPreparation::Atlas);
      verifyColoredAtlas(
          paint, assets,
          (std::filesystem::path{PLAYGROUND_COLOR_FONT}.parent_path() /
           "BungeeColor-Regular.ttf")
              .string(),
          'A', "A", TextPreparation::Atlas);
      verifyMixedText(paint, assets);
      auto emojiText = assets.getFont(FontProps{
          .path = std::string{PLAYGROUND_SOURCE_DIR} +
                  "/assets/fonts/Inter/Inter-Regular.ttf",
          .style = {.size = 24},
          .fallbacks = {{std::string{PLAYGROUND_SOURCE_DIR} +
                             "/assets/fonts/NotoEmoji/NotoColorEmoji.ttf",
                         {}}}});
      compareColorText(paint, emojiText, "A😀🧑🏽‍💻Z", 0,
                       {255, 255, 255, 255});
#endif
      {
        auto props =
            sdl::GPUDeviceProps{sdl::packagedShaderFormats(), true, driver};
        props.limits.maxResidentBytes = 4;
        props.limits.maxStreamBytes = 5632;
        const auto meshBytes =
            mesh->data().vertices.size() * sizeof(scene::Vertex3D) +
            mesh->data().indices.size() * sizeof(std::uint32_t);
        props.limits.maxMeshResidentBytes = meshBytes;
        auto limited = std::make_shared<GPUDevice>(props);
        GPUImagePreparer images{limited};
        auto makeSource = [] {
          SurfaceHandle surface{SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
                                SurfaceHandleDeleter{}};
          test::require(bool(surface), "residency test source");
          return makeSurfaceImage(std::move(surface));
        };
        auto sourceA = makeSource(), sourceB = makeSource();
        auto first = images.prepare(sourceA);
        test::require(images.prepare(sourceA) == first &&
                          images.residentBytes() == 4,
                      "resident upload reuses immutable identity");
        auto second = images.prepare(sourceB);
        test::require(
            images.residentBytes() == 4 &&
                first->pixelSize() == math::Size2{1, 1},
            "LRU eviction bounds residency without invalidating live handles");
        test::require(
            images.prepare(sourceA) != first,
            "evicted realization can be rebuilt while old handle stays valid");
        sourceA.reset();
        sourceB.reset();
        images.prune();
        test::require(images.residentBytes() == 0,
                      "expired CPU sources release residency");
        test::rejects<std::invalid_argument>(
            [&] { paint.images.prepare(first); },
            "foreign device images are rejected");
        PaintDevice streaming{limited};
        auto streamTarget = streaming.targets.color({8, 8});
        GPUPainter streamed{streaming, {1, 1}};
        streamed.fill(math::rect(0, 0, 8, 8), {255, 0, 0, 255});
        streamed.fill(math::rect(0, 0, 4, 8), {0, 255, 0, 255});
        streamed.fill(math::rect(0, 0, 2, 8), {0, 0, 255, 255});
        streamed.finish(streamTarget->get(), {8, 8}, {0, 0, 0, 255});
        test::require(streaming.stats().drawCalls == 2 &&
                          readPixel(limited, *streamTarget, 1, 4)[2] > .99f &&
                          readPixel(limited, *streamTarget, 3, 4)[1] > .99f &&
                          readPixel(limited, *streamTarget, 6, 4)[0] > .99f,
                      "cycled bounded stream preserves earlier batches and "
                      "painter order");
        GPUSceneRenderer boundedScene{streaming};
        auto another = scene::makeMesh(mesh->data());
        const std::array meshDraws{
            scene::MeshDraw{mesh, {{255, 0, 0, 255}}, {}},
            scene::MeshDraw{another, {{0, 255, 0, 255}}, {}}};
        auto boundedImage = std::dynamic_pointer_cast<const GPUImage>(
            boundedScene.render({{}, {8, 8}, {0, 0, 0, 255}}, meshDraws));
        test::require(boundedScene.meshResidentBytes() == meshBytes * 2,
                      "active draw resources exceed idle retention safely");
        boundedScene.trimUnused();
        test::require(boundedScene.meshResidentBytes() == meshBytes &&
                          readPixel(limited, *boundedImage, 4, 4)[0] > .99f,
                      "idle eviction preserves in-flight prepared geometry");
      }
      Window window{WindowConfig{.windowedSize = {32, 32}, .hidden = true}};
      const auto kind = rendering::GPUDriver::Vulkan;
      {
        sdl::GPURenderBackend backend{*window.get(), kind};
        backend.prepare({.scene3D = true, .linearComposition = true});
        auto frame = backend.beginFrame({.clearColor = {0, 0, 0, 255}});
        test::require(bool(frame), "GPU frame");
        frame->paint2D().fill(math::rect(0, 0, 32, 32), {255, 0, 0, 255});
        frame->present();
        test::rejects<std::logic_error>(
            [&] { frame->scene3D()->render({}, {}); },
            "scene service rejects use after presentation");
        test::rejects<std::logic_error>([&] { frame->present(); },
                                        "duplicate GPU present");
      }
      {
        auto software = sdl::createRenderBackend(
            *window.get(),
            rendering::RendererPreferences{rendering::RendererChoice::Software,
                                           rendering::GPUDriver::Auto, false},
            {});
        auto frame = software.backend->beginFrame({});
        frame->present();
      }
      test::require(SDL_WindowHasSurface(window.get()),
                    "GPU released window to software");
      test::require(SDL_DestroyWindowSurface(window.get()),
                    "release surface before GPU selection");
      auto restored = sdl::createRenderBackend(
          *window.get(),
          rendering::RendererPreferences{rendering::RendererChoice::SDLGPU,
                                         kind, false},
          {.scene3D = true});
      test::require(restored.state.selected.backend ==
                        rendering::RendererKind::SDLGPU,
                    "same native window supports returning to GPU");

      ui::UIRoot textRoot;
      textRoot.setContent(std::make_unique<ui::Text>(
          assets, ui::TextProps{.value = "I", .font = font}));
      textRoot.flushLayout(math::Size2{32, 32});
      textRoot.prepare({.images = &paint.images, .text = &paint.images});
      GPUPainter textPainter{paint, {1, 1}};
      textRoot.render(textPainter);
      textPainter.finish(target->get(), {32, 32}, {0, 0, 0, 0});
      SurfaceHandle cpu{SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32),
                        SurfaceHandleDeleter{}};
      test::require(bool(cpu), "software target");
      sdl::SurfacePainter cpuPainter{*cpu};
      textRoot.prepare({});
      textRoot.render(cpuPainter);
      paint.images.glyphAtlases = false;
      textRoot.prepare({.images = &paint.images, .text = &paint.images});
      GPUPainter compatibility{paint, {1, 1}};
      textRoot.render(compatibility);
      compatibility.finish(target->get(), {32, 32}, {0, 0, 0, 0});
    });
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
