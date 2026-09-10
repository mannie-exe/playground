#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_stdinc.h>

#include <study_sdl3/support/SDLError.hpp>

namespace study_sdl3::ui::events {
inline Uint32 registerButtonPressed() {
  const Uint32 event{SDL_RegisterEvents(1)};
  if (event == static_cast<Uint32>(-1))
    throwSDLError("Failed to register ButtonPressed event");
  return event;
}

inline Uint32 buttonPressed() {
  static const Uint32 event{registerButtonPressed()};
  return event;
}
}
