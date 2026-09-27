#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>

#include <platform/sdl/SDLGamepads.hpp>

namespace playground::sdl {
void SDLGamepads::handleEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_GAMEPAD_ADDED &&
      !_devices.contains(event.gdevice.which)) {
    GamepadResource device{SDL_OpenGamepad(event.gdevice.which)};
    if (device)
      _devices.emplace(event.gdevice.which, std::move(device));
    else
      SDL_Log("Cannot open gamepad: %s", SDL_GetError());
  } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
    _devices.erase(event.gdevice.which);
  }
}
} // namespace playground::sdl
