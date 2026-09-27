#pragma once

#include <functional>
#include <memory>

#include <SDL3/SDL_events.h>

namespace playground::sdl {

// Weak posting endpoints cannot outlive SDL. The event carries no owner
// pointer.
class EventWake {
  struct State;
  std::shared_ptr<State> _state;

public:
  EventWake();
  ~EventWake();

  EventWake(const EventWake &) = delete;
  EventWake &operator=(const EventWake &) = delete;

  std::function<void()> callback() const;
  bool consume(const SDL_Event &event);
};
} // namespace playground::sdl
