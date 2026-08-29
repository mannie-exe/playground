// std
#include <SDL3/SDL_timer.h>
#include <format>

// dependency includes
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <study_sdl3/app/SDLGuard.hpp>
#include <study_sdl3/app/TTFGuard.hpp>
#include <study_sdl3/platform/Window.hpp>
#include <study_sdl3/surface/Font.hpp>
#include <study_sdl3/surface/Image.hpp>
#include <study_sdl3/surface/Text.hpp>
#include <study_sdl3/ui/UI.hpp>

#ifndef PERFORMANCE
// #define PERFORMANCE
#endif

int main(int, char **) {
  int sdl_version = SDL_GetVersion();
  int image_version = IMG_Version();
  int ttf_version = TTF_Version();

  SDL_Log("%s", std::format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                            sdl_version, image_version, ttf_version)
                    .c_str());

  try {
    SDLGuard sdl{SDL_INIT_VIDEO};
    TTFGuard ttf;

    Window window = Window{"Sup", 750, 930};
    Image image{"C:\\Users\\intrn\\Downloads\\IMG_6239.PNG", true};
    Text text{"Wow!", Font("C:\\WINDOWS\\FONTS\\LBRITE.TTF", 42.0F),
              SDL_Color{255, 255, 0, 255}};
    UI ui{};

    // Loop state
    bool isRunning = true;
    SDL_Event event;
    uint64_t pollStart, pollDelta, drawStart, drawDelta, renderStart,
        renderDelta, offsetStart, offsetDelta, totalStart, totalDelta;
    while (isRunning) {

#ifdef PERFORMANCE
      offsetStart = uint64_t{SDL_GetPerformanceCounter()};
      offsetDelta = uint64_t{SDL_GetPerformanceCounter()} - offsetStart;

      totalStart = uint64_t{SDL_GetPerformanceCounter()};

      pollStart = uint64_t{SDL_GetPerformanceCounter()};
#endif
      // handle events
      while (SDL_PollEvent(&event)) {
        isRunning = !(event.type == SDL_EVENT_QUIT);

        if (!isRunning) {
          break;
        }

        // handleSDLEvent(event);
        // ui.handleEvent(event);
      }
#ifdef PERFORMANCE
      pollDelta =
          uint64_t{SDL_GetPerformanceCounter()} - pollStart - offsetDelta;

      drawStart = uint64_t{SDL_GetPerformanceCounter()};
#endif
      window.clearSurface(true);

#ifdef PERFORMANCE
      renderStart = uint64_t{SDL_GetPerformanceCounter()};
#endif
      // render
      // ui.render(*window.getSurface());
      image.render(*window.getSurface());
      // ui.render(*window.getSurface());
      text.render(*window.getSurface());
#ifdef PERFORMANCE
      renderDelta =
          uint64_t{SDL_GetPerformanceCounter()} - renderStart - offsetDelta;
#endif

      window.updateSurface();
#ifdef PERFORMANCE
      drawDelta =
          uint64_t{SDL_GetPerformanceCounter()} - drawStart - offsetDelta;

      totalDelta = uint64_t{SDL_GetPerformanceCounter()} - offsetStart;

      SDL_Log("%s",
              std::format(
                  "POLL: {} | RENDER: {} | DRAW: {} | TOTAL: {} | OFFSET: {}",
                  pollDelta, renderDelta, drawDelta, totalDelta, offsetDelta)
                  .c_str());
#endif
    }

    return 0;
  } catch (const std::string &err) {
    SDL_Log("%s", err.c_str());
    return 1;
  }
}
