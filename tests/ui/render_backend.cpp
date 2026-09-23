#include <memory>
#include <stdexcept>

#include <app/SDLGuard.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/SurfaceRenderBackend.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Test.hpp>
#include <ui/containers/Box.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

class RecordingPainter final : public ui::PaintContext {
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
    test::require(abstractBackend.drawableSize() == math::Vec2i{32, 24},
                  "backend queries physical size without creating a surface");
    test::require(!SDL_WindowHasSurface(window.get()),
                  "size queries do not select surface rendering");

    {
      auto frame = abstractBackend.beginFrame({.clearColor = {0, 0, 255, 255}});
      test::require(bool(frame), "hidden offscreen test window acquires frame");
      test::require(
          frame->scene3D() == nullptr,
          "software frame explicitly does not support scene rendering");
      test::rejects<std::logic_error>([&] { abstractBackend.beginFrame({}); },
                                      "overlapping frames rejected");
      session.render(frame->paint2D());
      auto &surface = *SDL_GetWindowSurface(window.get());
      const auto inside = pixel(surface, 2, 2);
      const auto outside = pixel(surface, 20, 20);
      test::require(inside.r == 255 && inside.b == 0 && outside.r == 0 &&
                        outside.b == 255,
                    "frame clears and native UI draws into backend target");
      frame->present();
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
    window.refreshState();
    test::require(
        window.state().actualSize == window.getWindowSize() &&
            !window.state().fullscreen,
        "observed window state is queried rather than copied from a request");
  });
}
