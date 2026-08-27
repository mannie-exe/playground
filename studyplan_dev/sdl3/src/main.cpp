// std
#include <SDL3/SDL_timer.h>
#include <format>
#include <memory>

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

void handleSDLEvent(SDL_Event &event) {
  // handle keyboard events
  if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
    // SDL_KeyboardEvent kbEvent = event.key;
    // const bool isUp =
    //     event.type == SDL_EVENT_KEY_UP || !kbEvent.down && !kbEvent.repeat;
    // SDL_Log("%llu kb @ type: %d -> mod: %d + key: %d (scancode: %d) | down: %
    // "
    //         "d repeat: %d up: %d",
    //         kbEvent.timestamp, kbEvent.type, kbEvent.mod, kbEvent.key,
    //         kbEvent.scancode, kbEvent.down, kbEvent.repeat, isUp);
  }

  // handle mouse events
  if (event.type == SDL_EVENT_MOUSE_MOTION ||
      event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
      event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
      event.type == SDL_EVENT_MOUSE_WHEEL) {
    // handle mouse motion
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      // SDL_MouseMotionEvent mMotionEvent = event.motion;
      // SDL_Log("%llu mMotion @ type: %d -> buttons: %d | %f, %f (%+f, %+f)",
      //         mMotionEvent.timestamp, mMotionEvent.type, mMotionEvent.state,
      //         mMotionEvent.x, mMotionEvent.y, mMotionEvent.xrel,
      //         mMotionEvent.yrel);
    }

    // handle mouse buttons
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      // SDL_MouseButtonEvent mButtonEvent = event.button;
      // const bool isUp = SDL_EVENT_MOUSE_BUTTON_UP || !mButtonEvent.down;
      // SDL_Log("%llu mButton @ type: %d -> buttons: %d | clicks: %d at (%f,
      // %f) "
      //         "| down: %d up: %d",
      //         mButtonEvent.timestamp, mButtonEvent.type, mButtonEvent.button,
      //         mButtonEvent.clicks, mButtonEvent.x, mButtonEvent.y,
      //         mButtonEvent.down, isUp);
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      // SDL_MouseWheelEvent mWheelEvent = event.wheel;
      // SDL_Log("%llu mWheel @ type: %d -> natural: %d | scroll: %+f %+f (%d, "
      //         "%d) @ (%f, %f)",
      //         mWheelEvent.timestamp, mWheelEvent.type,
      //         mWheelEvent.direction == SDL_MOUSEWHEEL_FLIPPED, mWheelEvent.x,
      //         mWheelEvent.y, mWheelEvent.integer_x, mWheelEvent.integer_y,
      //         mWheelEvent.mouse_x, mWheelEvent.mouse_y);
    }
  }

  // if (event.type == SDL_EVENT_MOUSE_MOTION) {
  //   // SDL_Log("Mouse moved");
  // } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
  //   // SDL_Log("Mouse clicked");
  //   SDL_Event quit = {.type = SDL_EVENT_QUIT};
  //   SDL_PushEvent(&quit);
  // } else if (event.type == SDL_EVENT_KEY_DOWN) {
  //   // SDL_Log("Keyboard button pressed");
  // }
}

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

    std::unique_ptr<Window> window = std::make_unique<Window>("Sup", 750, 930);
    std::unique_ptr<Image> image = std::make_unique<Image>(
        "C:\\Users\\intrn\\Downloads\\IMG_6239.PNG", true);
    std::unique_ptr<Font> font =
        std::make_unique<Font>("C:\\WINDOWS\\FONTS\\LBRITE.TTF", 42.0F);
    std::unique_ptr<Text> text = std::make_unique<Text>("Wow!", *font);
    std::unique_ptr<UI> uiManager = std::make_unique<UI>();

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
        // uiManager->handleEvent(event);
      }
#ifdef PERFORMANCE
      pollDelta =
          uint64_t{SDL_GetPerformanceCounter()} - pollStart - offsetDelta;

      drawStart = uint64_t{SDL_GetPerformanceCounter()};
#endif
      window->clear(true);

#ifdef PERFORMANCE
      renderStart = uint64_t{SDL_GetPerformanceCounter()};
      // render
      // uiManager.render(*window->getSurface());
#endif
      if (image) {
        image->render(*window->getSurface());
      }
      // if (uiManager) {
      //   uiManager->render(*window->getSurface());
      // }
      if (text) {
        text->render(*window->getSurface());
      }
#ifdef PERFORMANCE
      renderDelta =
          uint64_t{SDL_GetPerformanceCounter()} - renderStart - offsetDelta;
#endif

      window->update();
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
