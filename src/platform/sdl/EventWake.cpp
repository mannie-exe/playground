#include <mutex>
#include <stdexcept>

#include <platform/sdl/EventWake.hpp>

namespace playground::sdl {
struct EventWake::State {
  // SDL event filters may synchronously re-enter a posting endpoint.
  std::recursive_mutex mutex;
  Uint32 type{};
  bool pending{}, closed{};
};

EventWake::EventWake() : _state{std::make_shared<State>()} {
  _state->type = SDL_RegisterEvents(1);
  if (!_state->type)
    throw std::runtime_error("Cannot register runtime wake event");
}

EventWake::~EventWake() {
  std::lock_guard lock{_state->mutex};
  _state->closed = true;
}

std::function<void()> EventWake::callback() const {
  return [weak = std::weak_ptr{_state}] {
    if (auto state = weak.lock()) {
      std::lock_guard lock{state->mutex};
      if (state->closed || state->pending)
        return;
      SDL_Event event{};
      event.type = state->type;
      state->pending = true;
      state->pending = SDL_PushEvent(&event);
    }
  };
}

bool EventWake::consume(const SDL_Event &event) {
  if (event.type != _state->type)
    return false;
  std::lock_guard lock{_state->mutex};
  _state->pending = false;
  return true;
}
} // namespace playground::sdl
