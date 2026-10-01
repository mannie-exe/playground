// std
#include <exception>
#include <format>
#include <stdexcept>

// dependency includes
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_version.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <app/AppHost.hpp>

int main(int, char **) {
  try {
#ifdef __APPLE__
    // MoltenVK reads the native process environment. Set the default before
    // SDL starts threads or probes Vulkan; keep explicit developer overrides.
    if (SDL_setenv_unsafe("MVK_CONFIG_LOG_LEVEL", "2", 0) != 0)
      throw std::runtime_error("Cannot set the default MoltenVK log level");
#endif
    SDL_Log("%s", std::format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                              SDL_GetVersion(), IMG_Version(), TTF_Version())
                      .c_str());
    AppHost host;
    return host.run();
  } catch (const std::exception &err) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", err.what());
    return 1;
  } catch (...) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown application exception");
    return 1;
  }
}
