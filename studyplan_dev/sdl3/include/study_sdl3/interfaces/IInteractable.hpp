#pragma once

#include <format>
#include <string_view>

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

// keyboard
// mouse move
// mouse button
// mouse wheel
// window resize / pixel size changed
// window focus gained/lost
// mouse enter/leave
// text input
// gamepad/controller
// drop file
// touch/finger

enum class EventResult {
  Ignored,
  Handled,
  Consumed,
};

constexpr std::string_view toString(EventResult method) {
  switch (method) {
  case EventResult::Ignored:
    return "Ignored";
  case EventResult::Handled:
    return "Handled";
  case EventResult::Consumed:
    return "Consumed";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<EventResult> : std::formatter<std::string_view> {
  auto format(EventResult method, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(method), ctx);
  }
};

class IInteractable {
protected:
  IInteractable() = default;

public:
  virtual ~IInteractable() = default;

  virtual EventResult handleEvent(const SDL_Event &event) = 0;

  IInteractable(IInteractable &&) noexcept = default;
  IInteractable &operator=(IInteractable &&) noexcept = default;

  IInteractable(const IInteractable &) = delete;
  IInteractable &operator=(const IInteractable &) = delete;
};
