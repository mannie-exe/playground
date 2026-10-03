#pragma once

#include <SDL3/SDL_stdinc.h>

#include <support/SDLError.hpp>

namespace playground::sdl {
// Entry-point setup only, before SDL probes drivers or starts worker threads.
// Native environment defaults preserve explicit developer overrides.
inline void configureProcessEnvironment() {
#ifdef __APPLE__
  if (SDL_setenv_unsafe("MVK_CONFIG_LOG_LEVEL", "2", 0) != 0)
    throwSDLError("Cannot set default MoltenVK log level");
#endif
}
} // namespace playground::sdl
