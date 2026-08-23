#define SDL_MAIN_USE_CALLBACKS 1

// std
#include <format>

// dependency includes
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <Window.hpp>

using namespace std;

SDL_AppResult SDL_AppInit(void **appState, int argc, char *argv[]) {
  int sdl_version = SDL_GetVersion();
  int image_version = IMG_Version();
  int ttf_version = TTF_Version();

  bool didSdlFail = sdl_version == 0 || image_version == 0 || ttf_version == 0;
  if (didSdlFail)
    return SDL_APP_FAILURE;

  SDL_Log("%s", format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                       sdl_version, image_version, ttf_version)
                    .c_str());

  SDL_InitSubSystem(SDL_INIT_VIDEO);

  *appState = new Window("Sup");
  // ALSO WORKS
  // auto state = make_unique<Window>("Sup");
  // *appState = state.release();

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appState, SDL_Event *event) {
  bool didExit = event->type == SDL_EVENT_QUIT;

  if (didExit) {
    return SDL_APP_SUCCESS;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appState) { return SDL_APP_CONTINUE; }

void SDL_AppQuit(void *appState, SDL_AppResult result) {
  delete reinterpret_cast<Window *>(appState);
  // ALSO WORKS
  // unique_ptr<Window> state{reinterpret_cast<Window *>(appState)};
}
