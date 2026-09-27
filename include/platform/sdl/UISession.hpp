#pragma once

#include <platform/Presentation.hpp>
#include <platform/sdl/EventResult.hpp>
#include <platform/sdl/SDLInput.hpp>
#include <rendering/RenderBackend.hpp>
#include <runtime/Activity.hpp>
#include <ui/UIRoot.hpp>

class PerformanceMonitor;
class AppContext;

namespace playground::sdl {
class WindowServices;
enum class UISessionTiming { CallerDelta, Monotonic };

// App-owned bridge. No transient AppContext is stored in a node or callback.
class UISession {
  ui::UIRoot _root;
  ui::LayoutEnvironment _environment;
  platform::WindowMetrics _metrics{};
  platform::ViewportMapping _mapping{};
  PerformanceMonitor *_monitor{};
  ui::UIWorkSample _reported;
  WindowServices *_windowServices{};
  ui::Connection _attachment;
  runtime::ActivityClock::time_point _lastUpdate{runtime::ActivityClock::now()};
  UISessionTiming _timing{UISessionTiming::CallerDelta};

  void reportWork();

public:
  explicit UISession(UISessionTiming timing = UISessionTiming::CallerDelta)
      : _timing{timing} {}

  ui::UIRoot &root() noexcept { return _root; }

  const ui::UIRoot &root() const noexcept { return _root; }

  void synchronize(AppContext &context);
  void synchronize(platform::WindowMetrics metrics,
                   const platform::ViewportProps &props,
                   PerformanceMonitor *monitor = nullptr);
  EventResult handleEvent(const SDL_Event &event);
  void update(float seconds);

  runtime::ActivityDemand activityDemand();

  void render(rendering::PaintContext &context,
              scene::SceneRenderer *scenes = nullptr);

  void render(rendering::RenderFrame &frame) {
    render(frame.paint2D(), frame.scene3D());
  }

  void clear() {
    _attachment.disconnect();
    _windowServices = nullptr;
    _root.setContent({});
  }
};
} // namespace playground::sdl
