#include <memory>
#include <stdexcept>

#include <app/SDLGuard.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/RenderBackendFactory.hpp>
#include <platform/sdl/RenderError.hpp>
#include <platform/sdl/SurfaceRenderBackend.hpp>
#include <platform/sdl/UISession.hpp>
#include <scene/SceneRenderer.hpp>
#include <support/Test.hpp>
#include <ui/containers/Box.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

class RecordingPainter final : public rendering::PaintContext {
public:
  int depth{}, fills{};

  void save() override { ++depth; }

  void restore() noexcept override { --depth; }

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect, math::ColorRGBA8) override { ++fills; }
};

static math::ColorRGBA8 pixel(SDL_Surface &surface, int x, int y) {
  math::ColorRGBA8 color;
  test::require(SDL_ReadSurfacePixel(&surface, x, y, &color.r, &color.g,
                                     &color.b, &color.a),
                "read target pixel");
  return color;
}

int main() {
  return test::run([] {
    sdl::UISession session;
    auto container = std::make_unique<ui::Box>();
    container->setChild(std::make_unique<ui::Rectangle>(
        ui::RectangleProps{.fill = {255, 0, 0, 255}},
        layout::BoxProps{.width = layout::SizeRule::fixed(8),
                         .height = layout::SizeRule::fixed(8)}));
    session.root().setContent(std::move(container));
    session.synchronize({.windowSize = {32, 24}, .drawableSize = {32, 24}}, {});

    RecordingPainter recording;
    session.render(recording);
    test::require(recording.fills == 1 && recording.depth == 0,
                  "session uses a borrowed non-SDL painter and restores state");

    SDLGuard sdl{SDL_INIT_VIDEO};
    Window window{WindowConfig{.windowedSize = {32, 24}, .hidden = true}};
    test::require(!SDL_WindowHasSurface(window.get()),
                  "constructing a window does not select surface rendering");
    sdl::SurfaceRenderBackend backend{*window.get()};
    rendering::RenderBackend &abstractBackend = backend;
    abstractBackend.prepare({.scene3D = true});
    test::rejects<std::runtime_error>(
        [&] { abstractBackend.prepare({.linearComposition = true}); },
        "required unsupported capabilities fail before activation");
    test::require(abstractBackend.description().capabilities.paint2D &&
                      abstractBackend.description().capabilities.scene3D,
                  "backend advertises implemented capabilities only");
    const auto selected =
        sdl::resolveRenderer({rendering::RendererChoice::Software}, {});
    test::require(!selected.isFallback() &&
                      selected.selected.backend ==
                          rendering::RendererKind::Software,
                  "explicit software selection remains deterministic");
    // CTest uses SDL's dummy video driver, which cannot load Vulkan.
    for (const bool create : {false, true}) {
      const rendering::RendererPreferences preferences{
          rendering::RendererChoice::SDLGPU, rendering::GPUDriver::Vulkan, false};
      const rendering::RendererRequirements requirements{
          .scene3D = true, .linearComposition = true, .metallicRoughness = true};
      std::string message;
      try {
        if (create)
          sdl::createRenderBackend(*window.get(), preferences, requirements);
        else
          sdl::resolveRenderer(preferences, requirements);
      } catch (const std::runtime_error &error) {
        message = error.what();
      }
      test::require(
          message.find("Vulkan loader unavailable:") != std::string::npos ||
              message.find("Vulkan shaders are not packaged") != std::string::npos,
          "GPU selection and creation report runtime or shader prerequisites");
    }
    test::rejects<std::runtime_error>(
        [] {
          sdl::resolveRenderer({rendering::RendererChoice::Software,
                                rendering::GPUDriver::Auto, false},
                               {.linearComposition = true});
        },
        "factory rejects unsupported software composition needs");
    auto created = sdl::createRenderBackend(*window.get(), selected);
    {
      rendering::RenderBackendProps limited{
          .allocations = {.maxTargetBytes = 16}};
      auto constrained =
          sdl::createRenderBackend(*window.get(), selected, limited);
      test::rejects<std::length_error>(
          [&] { constrained->beginFrame({}); },
          "factory threads per-target policy into software frames");
      SDL_SetError("test native presentation");
      try {
        sdl::throwRenderError("test context",
                              rendering::RenderOperation::Present);
      } catch (const rendering::RenderFailure &failure) {
        test::require(
            failure.operation() == rendering::RenderOperation::Present &&
                std::string{failure.what()}.find("test native presentation") !=
                    std::string::npos,
            "typed native failure retains operation and diagnostic");
      }
    }
    test::require(created->description().capabilities ==
                      selected.selected.capabilities,
                  "factory realization matches negotiated capabilities");
    auto forged = selected;
    forged.selected.capabilities.scene3D = false;
    test::rejects([&] { sdl::createRenderBackend(*window.get(), forged); },
                  "factory does not accept invented capabilities");
    test::require(abstractBackend.drawableSize() == math::Vec2i{32, 24},
                  "backend queries physical size without creating a surface");
    test::require(!SDL_WindowHasSurface(window.get()),
                  "size queries do not select surface rendering");

    {
      auto frame = abstractBackend.beginFrame({.clearColor = {0, 0, 255, 255}});
      test::require(bool(frame), "hidden offscreen test window acquires frame");
      test::require(frame->scene3D() != nullptr,
                    "software frame provides scene rendering");
      test::rejects<std::logic_error>([&] { abstractBackend.beginFrame({}); },
                                      "overlapping frames rejected");
      session.render(frame->paint2D());
      auto &surface = *SDL_GetWindowSurface(window.get());
      const auto inside = pixel(surface, 2, 2);
      const auto outside = pixel(surface, 20, 20);
      test::require(inside.r == 255 && inside.b == 0 && outside.r == 0 &&
                        outside.b == 255,
                    "frame clears and native UI draws into backend target");
      test::require(frame->present() ==
                        rendering::PresentationOutcome::Submitted,
                    "software presentation reports accepted frame");
      test::require(abstractBackend.completedWork() == 1,
                    "software completed work advances after successful update");
      test::rejects<std::logic_error>(
          [&] { frame->scene3D()->render({}, {}); },
          "scene service is frame scoped after presentation");
      test::rejects<std::logic_error>([&] { frame->present(); },
                                      "duplicate presentation rejected");
      test::rejects<std::logic_error>([&] { frame->paint2D(); },
                                      "painting after presentation rejected");
    }

    auto *surface = SDL_GetWindowSurface(window.get());
    const SDL_Rect originalClip{2, 2, 4, 4};
    test::require(SDL_SetSurfaceClipRect(surface, &originalClip), "set clip");
    test::rejects<std::runtime_error>(
        [&] {
          auto frame = backend.beginFrame({.clearColor = {0, 255, 0, 255}});
          frame->paint2D().translate({3, 4});
          throw std::runtime_error("simulated paint failure");
        },
        "exception abandons frame");
    SDL_Rect restored{};
    test::require(SDL_GetSurfaceClipRect(surface, &restored) &&
                      restored.x == 2 && restored.y == 2 && restored.w == 4 &&
                      restored.h == 4,
                  "abandoned frame restores borrowed surface state");
    test::require(pixel(*surface, 20, 20).g == 255,
                  "clear replaces the entire target despite previous clipping");
    {
      auto next = backend.beginFrame({});
      next->present();
    }

    window.setWindowedSize({48, 36});
    test::require(backend.drawableSize() == math::Vec2i{48, 36},
                  "resize reflected without retaining old surface");
    auto resized = backend.beginFrame({.clearColor = {255, 255, 255, 255}});
    test::require(bool(resized), "frame reacquires resized target");
    surface = SDL_GetWindowSurface(window.get());
    test::require(surface->w == 48 && surface->h == 36 &&
                      pixel(*surface, 47, 35).r == 255,
                  "resized frame clears new target extent");
    resized->present();
    resized.reset();
    {
      auto scaled = backend.beginFrame({.clearColor = {1, 2, 3, 255},
                                        .settings = {.resolutionScale = 0.5f}});
      test::require(scaled->paint2D().pixelScale() == math::Vec2f{0.5f, 0.5f},
                    "internal resolution changes raster density only");
      scaled->paint2D().fill({{}, {48, 36}}, {255, 0, 0, 255});
      scaled->present();
      test::require(pixel(*SDL_GetWindowSurface(window.get()), 47, 35).r == 255,
                    "scaled frame is composited over the whole window");
    }
    test::rejects(
        [&] { backend.beginFrame({.settings = {.resolutionScale = 0}}); },
        "invalid frame scale rejected");
    window.applyPreferences(
        {.display = {.selection = platform::DisplaySelection::Primary},
         .center = false});
    window.advanceTransition(platform::WindowTransition::Clock::now());
    test::require(window.requestStatus().outcome ==
                      platform::WindowTransitionOutcome::Observed,
                  "normal window presentation does not require native restoration");
    window.refreshState();
    test::require(
        window.state().actualSize == window.getWindowSize() &&
            !window.state().fullscreen,
        "observed window state is queried rather than copied from a request");

    const WindowRequest resize{.preferences = {.center = false},
                               .windowedSize = math::Vec2i{64, 48}};
    const auto first = window.requestPreferences(resize);
    test::require(window.pendingRequest()->windowedSize == resize.windowedSize,
                  "pending request retains normal geometry");
    window.applyPreferences({.center = false});
    test::require(window.requestStatus().generation > first &&
                      window.pendingRequest()->windowedSize == resize.windowedSize,
                  "presentation supersession preserves pending normal size");
    window.advanceTransition(platform::WindowTransition::Clock::now());
    test::require(window.state().windowedSize == *resize.windowedSize &&
                      window.requestStatus().outcome ==
                          platform::WindowTransitionOutcome::Observed,
                  "superseded resize reaches requested size");

    window.requestPreferences(resize);
    const auto generation = window.requestStatus().generation;
    auto invalid = resize;
    invalid.minimumSize = {0, 1};
    test::rejects([&] { window.requestPreferences(invalid); },
                  "invalid request fails before superseding pending work");
    test::require(window.requestStatus().generation == generation &&
                      window.pendingRequest().has_value(),
                  "preflight failure retains pending request");
    window.cancelTransition();
    test::require(!window.pendingRequest() && !window.transitionWakeAt() &&
                      !window.advanceTransition(platform::WindowTransition::Clock::now()),
                  "cancellation prevents further adapter work");

    // The dummy backend accepts normal geometry but cannot maximize windows.
    window.requestPreferences(
        {.preferences = {.mode = platform::WindowMode::Maximized, .center = false},
         .windowedSize = math::Vec2i{80, 60}});
    window.advanceTransition(platform::WindowTransition::Clock::now());
    test::require(window.requestStatus().outcome ==
                      platform::WindowTransitionOutcome::Failed &&
                      !window.pendingRequest() && !window.transitionWakeAt(),
                  "native mode failure terminates the request");
    test::require(window.state().windowedSize == math::Vec2i{80, 60},
                  "late mode failure preserves already observed normal geometry");
    window.applyPreferences({.center = false});
    window.advanceTransition(platform::WindowTransition::Clock::now());
    test::require(window.requestStatus().outcome ==
                      platform::WindowTransitionOutcome::Observed,
                  "rejected native mode does not require undoing an accepted request");
  });
}
