// std
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
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

#ifndef DEMO
#define DEMO
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

#ifdef DEMO
    Window window = Window{"Sup", SDL_Point{750, 930}, SDL_WINDOW_RESIZABLE};
    Image image{std::string(SDL_GetBasePath()) +
                    "assets/images/demo/IMG_6239.PNG",
                true};
    Text text{{.value = "Wow!",
               .style = {.fgColor = SDL_Color{255, 255, 0, 255}},
               .layout = {.scaleWidth = window.getSurfaceSize().x}},
              Font(FontProps{.path = std::string(SDL_GetBasePath()) +
                                     "assets/fonts/LBRITE.TTF",
                             .style = {.size = 42.0f}})};
    UI ui{};
#else
    Window window = Window{"Sup", SDL_Point{750, 930}, SDL_WINDOW_RESIZABLE};
#endif

    // Loop state
    bool isRunning = true;
    SDL_Event event;
#ifdef PERFORMANCE
    uint64_t pollStart, pollDelta, drawStart, drawDelta, renderStart,
        renderDelta, offsetStart, offsetDelta, totalStart, totalDelta;
#endif
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

#ifdef DEMO
        ui.handleEvent(event);
#endif

        if (event.type == SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
#ifdef DEMO
          text.setScaleWidth(window.getSurfaceSize().x);
#endif
        }
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
#ifdef DEMO
      image.render(*window.getSurface());
      text.render(*window.getSurface());
      ui.render(*window.getSurface());
#endif
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
