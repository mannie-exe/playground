// std
#include <format>

// dependency includes
#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <app/AppHost.hpp>

int main(int, char **) {
  int sdl_version = SDL_GetVersion();
  int image_version = IMG_Version();
  int ttf_version = TTF_Version();

  SDL_Log("%s", std::format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                            sdl_version, image_version, ttf_version)
                    .c_str());

  try {
    AppHost host;
    return host.run();
  } catch (const std::string &err) {
    SDL_Log("%s", err.c_str());
    return 1;
  }
}
