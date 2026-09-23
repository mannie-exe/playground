// std
#include <exception>
#include <format>

// dependency includes
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_version.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <app/AppHost.hpp>

int main(int, char **) {
  try {
    SDL_Log("%s", std::format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                              SDL_GetVersion(), IMG_Version(), TTF_Version())
                      .c_str());
    AppHost host;
    return host.run();
  } catch (const std::exception &err) {
    SDL_Log("%s", err.what());
    return 1;
  } catch (...) {
    SDL_Log("Unknown application exception");
    return 1;
  }
}
