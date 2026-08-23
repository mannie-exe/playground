// std
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <format>

// dependency includes
#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// project-local includes
#include <UI.hpp>
#include <Window.hpp>

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

  bool didSdlFail = sdl_version == 0 || image_version == 0 || ttf_version == 0;
  if (didSdlFail)
    return SDL_APP_FAILURE;

  SDL_Log("%s", std::format("SDL loaded: SDL v{}, SDL_image v{}, SDL_ttf v{}",
                            sdl_version, image_version, ttf_version)
                    .c_str());

  std::string err;
  Window *window{nullptr};

  SDL_Init(SDL_INIT_VIDEO);
  err = std::string{SDL_GetError()};
  if (!err.empty()) {
    SDL_Log("Failed to initialize SDL3:\n  %s", err.c_str());
    SDL_ClearError();
    return 1;
  }

  try {
    window = new Window{"Sup"};
  } catch (std::string) {
  }
  if (!window) {
    return 1;
  }

  // App state
  UI uiManager;

  // Loop state
  bool isRunning = true;
  SDL_Event event;
  while (isRunning) {
    // SDL_PumpEvents(); // NOT NEEDED with SDL_PollEvent explicit event handle

    while (SDL_PollEvent(&event)) {
      isRunning = !(event.type == SDL_EVENT_QUIT);

      if (!isRunning) {
        break;
      }

      // iterate
      // handleSDLEvent(event);
      uiManager.handleEvent(event);
    }

    // Render
    window->clear(true);
    uiManager.render(*window->getSurface());
    window->update();
  }

  delete window;

  SDL_Quit();
  return 0;
}
