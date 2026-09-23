#include <climits>
#include <limits>
#include <platform/Presentation.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Test.hpp>
#include <ui/containers/Box.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/Button.hpp>

using namespace playground;
int main() {
  return test::run([] {
    platform::AppViewPolicy sizing{
        .initialSizing = platform::InitialWindowSizing::FitContent};
    test::require(
        sizing.initialWindowSize({800, 600}) == math::Vec2i{800, 600},
        "content-sized apps need no duplicated preferred window size");
    sizing.preferredWindowSize = math::Vec2i{640, 480};
    test::require(sizing.initialWindowSize({800, 600}) == math::Vec2i{640, 480},
                  "explicit sizing policy supplies its own fallback");
    sizing.preferredWindowSize = math::Vec2i{0, 480};
    test::rejects([&] { sizing.validate(); },
                  "nonpositive authored window size rejected");
    test::rejects([] { platform::AppViewPolicy{}.initialWindowSize({0, 600}); },
                  "invalid bootstrap rejected");
    platform::ViewportProps props;
    auto windows =
        platform::resolveViewport(props, {{1600, 1200}, {1600, 1200}, 2});
    auto mac = platform::resolveViewport(props, {{800, 600}, {1600, 1200}, 2});
    test::require(windows.logicalSize == math::Size2{800, 600} &&
                      mac.logicalSize == windows.logicalSize,
                  "OS coordinate conventions yield the same logical UI size");
    test::require(windows.pixelsPerLogical == math::Vec2f{2, 2} &&
                      mac.pixelsPerLogical == windows.pixelsPerLogical,
                  "same physical UI density");
    props.followSystemScale = false;
    test::require(
        platform::resolveViewport(props, {{800, 600}, {1600, 1200}, 2})
                .logicalSize == math::Size2{800, 600},
        "manual scale uses window coordinates");
    props.mode = platform::ViewportMode::FixedCanvas;
    props.canvasSize = {100, 100};
    for (const auto fit :
         {platform::ViewportFit::Contain, platform::ViewportFit::Cover,
          platform::ViewportFit::Stretch}) {
      props.fit = fit;
      const auto mapping =
          platform::resolveViewport(props, {{200, 100}, {400, 200}, 2});
      test::require(mapping.logicalSize == props.canvasSize,
                    "fixed canvas preserves logical size");
      for (int i = 0; i < 100; ++i) {
        math::Point2 point{static_cast<float>(i), static_cast<float>(99 - i)};
        const math::Point2 projected{
            point.x * mapping.windowUnitsPerLogical.x + mapping.offset.x,
            point.y * mapping.windowUnitsPerLogical.y + mapping.offset.y};
        test::require(mapping.toLogical(projected) == point,
                      "pointer/paint transform round trip");
      }
      if (fit == platform::ViewportFit::Contain)
        test::require(mapping.offset == math::Vec2f{50, 0},
                      "contain letterboxes");
      if (fit == platform::ViewportFit::Cover)
        test::require(mapping.offset == math::Vec2f{0, -50}, "cover crops");
      if (fit == platform::ViewportFit::Stretch)
        test::require(mapping.windowUnitsPerLogical == math::Vec2f{2, 1},
                      "stretch is anisotropic");
    }
    auto minimized = platform::resolveViewport(props, {{0, 0}, {0, 0}, 1});
    test::require(math::isFinite(minimized.pixelsPerLogical),
                  "minimized mapping remains usable");
    props.scale = std::numeric_limits<float>::quiet_NaN();
    test::rejects([&] { props.validate(); }, "invalid scale rejected");
    test::require(platform::RenderSettings{0.5f}.targetSize({101, 99}) ==
                      math::Vec2i{51, 50},
                  "raster size rounds up");
    test::rejects([] { platform::RenderSettings{0}.validate(); },
                  "zero render scale rejected");
    test::rejects<std::overflow_error>(
        [] { platform::RenderSettings{4}.targetSize({INT_MAX, 1}); },
        "extent overflow rejected");

    ui::UIRoot root;
    auto box = std::make_unique<ui::Box>(
        layout::BoxProps{.padding = math::Insets::all(24)});
    box->setChild(std::make_unique<ui::Rectangle>(
        ui::RectangleProps{},
        layout::BoxProps{.width = layout::SizeRule::fixed(750),
                         .height = layout::SizeRule::fixed(930)}));
    root.setContent(std::move(box));
    test::require(root.preferredSize({2000, 2000}) == math::Size2{798, 978},
                  "preferred size includes padding once");
    root.flushLayout({400, 300});
    const auto bounds = root.content()->bounds();
    test::require(
        root.preferredSize({2000, 2000}) == math::Size2{798, 978} &&
            root.content()->bounds() == bounds,
        "preferred query does not arrange or depend on current tight viewport");
    test::require(root.preferredSize({200, 150}) == math::Size2{200, 150},
                  "work-area offer bounds preferred size");
    test::rejects([&] { root.preferredSize({0, 10}); },
                  "invalid measurement offer rejected");

    sdl::UISession session;
    auto button = std::make_unique<ui::Button>();
    auto *control = button.get();
    int activations{};
    auto connection = control->onActivate([&] { ++activations; });
    session.root().setContent(std::move(button));
    session.synchronize({{400, 200}, {800, 400}, 2},
                        {.mode = platform::ViewportMode::FixedCanvas,
                         .canvasSize = {100, 100}});
    auto click = [&](SDL_EventType type, float x, float y) {
      SDL_Event event{};
      event.button.type = type;
      event.button.which = 1;
      event.button.button = 1;
      event.button.x = x;
      event.button.y = y;
      session.handleEvent(event);
    };
    click(SDL_EVENT_MOUSE_BUTTON_DOWN, 150, 100);
    test::require(control->isPressed(),
                  "window coordinates map into the fixed canvas");
    click(SDL_EVENT_MOUSE_BUTTON_UP, 50, 100);
    test::require(!control->isPressed() && activations == 0,
                  "captured release in letterbox cancels activation");
    click(SDL_EVENT_MOUSE_BUTTON_DOWN, 50, 100);
    test::require(!control->isPressed(), "letterbox cannot start a press");
    click(SDL_EVENT_MOUSE_BUTTON_DOWN, 150, 100);
    click(SDL_EVENT_MOUSE_BUTTON_UP, 150, 100);
    test::require(activations == 1, "mapped click activates exactly once");
    SDL_Event touch{};
    touch.tfinger.type = SDL_EVENT_FINGER_DOWN;
    touch.tfinger.fingerID = 7;
    touch.tfinger.x = 0.5f;
    touch.tfinger.y = 0.5f;
    session.handleEvent(touch);
    touch.tfinger.type = SDL_EVENT_FINGER_UP;
    session.handleEvent(touch);
    test::require(activations == 2,
                  "normalized touch uses window extent before inverse mapping");
  });
}
