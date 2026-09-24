#pragma once

#include <optional>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <ui/UITypes.hpp>

namespace playground::sdl {
ui::Key toUIKey(SDL_Keycode value);
// Positions/deltas are in window coordinates. UISession applies the viewport
// inverse afterward; normalized touch must use the native window extent here.
std::optional<ui::UIEvent> toUIEvent(const SDL_Event &event,
                                     math::Size2 windowSize);
} // namespace playground::sdl
