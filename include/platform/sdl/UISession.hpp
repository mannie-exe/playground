#pragma once

#include <platform/Presentation.hpp>
#include <platform/sdl/EventResult.hpp>
#include <platform/sdl/SDLInput.hpp>
#include <rendering/RenderBackend.hpp>
#include <ui/UIRoot.hpp>

namespace playground::sdl {
// App-owned bridge. No transient AppContext is stored in a node or callback.
class UISession {
  ui::UIRoot _root;
  ui::LayoutEnvironment _environment;
  platform::WindowMetrics _metrics{};
  platform::ViewportMapping _mapping{};

public:
  ui::UIRoot &root() noexcept { return _root; }
  const ui::UIRoot &root() const noexcept { return _root; }
  void synchronize(platform::WindowMetrics metrics,
                   const platform::ViewportProps &props);
  EventResult handleEvent(const SDL_Event &event);
  void update(float seconds) { _root.update(seconds); }
  void render(rendering::PaintContext &context,
              scene::SceneRenderer *scenes = nullptr);
  void render(rendering::RenderFrame &frame) {
    render(frame.paint2D(), frame.scene3D());
  }
  void clear() { _root.setContent({}); }
};
} // namespace playground::sdl
