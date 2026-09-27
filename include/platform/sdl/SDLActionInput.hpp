#pragma once

#include <optional>

#include <SDL3/SDL_events.h>

#include <input/InputMap.hpp>

namespace playground::sdl {
std::optional<input::InputEvent> toActionInput(const SDL_Event &);
// Cancellation events are still forwarded to UI for focus/capture cleanup.
void cancelActionInput(input::InputMap &, const SDL_Event &);
} // namespace playground::sdl
