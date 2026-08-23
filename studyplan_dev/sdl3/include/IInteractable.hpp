#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

class IInteractable {
public:
  IInteractable() = default;
  virtual ~IInteractable() = default;

  virtual void handleEvent(const SDL_Event &event);

  virtual void onKey(SDL_KeyboardEvent kbEvent) {};

  virtual void onMouseMove(SDL_MouseMotionEvent mMotionEvent) {};
  virtual void onMouseClick(SDL_MouseButtonEvent mButtonEvent) {};
  virtual void onMouseWheel(SDL_MouseWheelEvent mWheelEvent) {};

  IInteractable(const IInteractable &) = delete;
  IInteractable &operator=(const IInteractable &) = delete;
};
