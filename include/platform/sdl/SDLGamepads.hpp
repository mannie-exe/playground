#pragma once

#include <map>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <support/SDLResource.hpp>

namespace playground::sdl {
// SDL only emits gamepad input for opened devices. Own them until removal.
class SDLGamepads {
  using GamepadResource = SDLResource<SDL_Gamepad, SDL_CloseGamepad>;
  std::map<SDL_JoystickID, GamepadResource> _devices;

public:
  void handleEvent(const SDL_Event &);
};
} // namespace playground::sdl
