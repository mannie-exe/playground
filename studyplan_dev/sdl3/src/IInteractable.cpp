#include <IInteractable.hpp>

void IInteractable::handleEvent(const SDL_Event &event) {
  // handle keyboard events
  if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
    SDL_KeyboardEvent kbEvent = event.key;
    // const bool isUp =
    //     event.type == SDL_EVENT_KEY_UP || !kbEvent.down && !kbEvent.repeat;
    // SDL_Log("%llu kb @ type: %d -> mod: %d + key: %d (scancode: %d) |"
    //         "down: %d repeat: %d up: %d",
    //         kbEvent.timestamp, kbEvent.type, kbEvent.mod, kbEvent.key,
    //         kbEvent.scancode, kbEvent.down, kbEvent.repeat, isUp);
    onKey(kbEvent);
  }

  // handle mouse events
  if (event.type == SDL_EVENT_MOUSE_MOTION ||
      event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
      event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
      event.type == SDL_EVENT_MOUSE_WHEEL) {
    // handle mouse motion
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      SDL_MouseMotionEvent mMotionEvent = event.motion;
      // SDL_Log("%llu mMotion @ type: %d -> buttons: %d | %f, %f (%+f, %+f)",
      //         mMotionEvent.timestamp, mMotionEvent.type,
      //         mMotionEvent.state, mMotionEvent.x, mMotionEvent.y,
      //         mMotionEvent.xrel, mMotionEvent.yrel);
      onMouseMove(mMotionEvent);
    }

    // handle mouse buttons
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      SDL_MouseButtonEvent mButtonEvent = event.button;
      // const bool isUp = SDL_EVENT_MOUSE_BUTTON_UP || !mButtonEvent.down;
      // SDL_Log("%llu mButton @ type: %d -> buttons: %d | clicks: %d at (%f,
      // %f) "
      //         "| down: %d up: %d",
      //         mButtonEvent.timestamp, mButtonEvent.type,
      //         mButtonEvent.button, mButtonEvent.clicks, mButtonEvent.x,
      //         mButtonEvent.y, mButtonEvent.down, isUp);
      onMouseClick(mButtonEvent);
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      SDL_MouseWheelEvent mWheelEvent = event.wheel;
      // SDL_Log("%llu mWheel @ type: %d -> natural: %d | scroll: %+f %+f (%d,
      // "
      //         "%d) @ (%f, %f)",
      //         mWheelEvent.timestamp, mWheelEvent.type,
      //         mWheelEvent.direction == SDL_MOUSEWHEEL_FLIPPED,
      //         mWheelEvent.x, mWheelEvent.y, mWheelEvent.integer_x,
      //         mWheelEvent.integer_y, mWheelEvent.mouse_x,
      //         mWheelEvent.mouse_y);
      onMouseWheel(mWheelEvent);
    }
  }
}
