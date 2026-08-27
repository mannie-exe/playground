#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

class IInteractable {
public:
  IInteractable() = default;
  virtual ~IInteractable() = default;

  virtual void handleEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
      const SDL_KeyboardEvent &kbEvent = event.key;
      onKey(kbEvent);
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION ||
        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
        event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (event.type == SDL_EVENT_MOUSE_MOTION) {
        const SDL_MouseMotionEvent &mMotionEvent = event.motion;
        onMouseMove(mMotionEvent);
      }

      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
          event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const SDL_MouseButtonEvent &mButtonEvent = event.button;
        onMouseClick(mButtonEvent);
      }

      if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        const SDL_MouseWheelEvent &mWheelEvent = event.wheel;
        onMouseWheel(mWheelEvent);
      }
    }
  }

  virtual void onKey(const SDL_KeyboardEvent &kbEvent) {};

  virtual void onMouseMove(const SDL_MouseMotionEvent &mMotionEvent) {};
  virtual void onMouseClick(const SDL_MouseButtonEvent &mButtonEvent) {};
  virtual void onMouseWheel(const SDL_MouseWheelEvent &mWheelEvent) {};

  IInteractable(const IInteractable &) = delete;
  IInteractable &operator=(const IInteractable &) = delete;
};
