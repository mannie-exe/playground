#pragma once

#include <functional>
#include <memory>

#include <SDL3/SDL_video.h>

#include <platform/Presentation.hpp>
#include <runtime/ActivationLifetime.hpp>
#include <ui/UIRoot.hpp>

namespace playground::sdl {
// Native services outlive attached app UI, but are destroyed before SDL_Window.
class WindowServices {
  struct Impl;
  std::shared_ptr<Impl> _impl;

public:
  explicit WindowServices(SDL_Window *window);
  ~WindowServices();
  WindowServices(const WindowServices &) = delete;
  WindowServices &operator=(const WindowServices &) = delete;
  ui::Connection attach(ui::UIRoot &root);
  void publish(ui::UIRoot &root, const platform::ViewportMapping &mapping);
  void pump();
  // Cancel active UI gestures/composition while retaining logical focus.
  void cancelInput();
  ui::Connection lockRelativeMouse(runtime::ActivationToken owner);
  void releaseRelativeMouse() noexcept;
  bool relativeMouseActive() const noexcept;
  void setMode(ui::AccessibilityMode);
  void setWakeCallback(std::function<void()> callback);
};
} // namespace playground::sdl
