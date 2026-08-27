#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>

#include <study_sdl3/ui/DisplayObject.hpp>
#include <study_sdl3/ui/Rectangle.hpp>

class Button : public Rectangle {
  DisplayObject &_parent;

public:
  Button(const SDL_Rect &rect, DisplayObject &parent)
      : Rectangle{rect}, _parent{parent} {}

  virtual void onMouseEnter() override {}

  virtual void onMouseExit() override {}

  void onMouseClick(const SDL_MouseButtonEvent &mButtonEvent) override {
    if (!_hover)
      return;

    if (!mButtonEvent.down)
      return;

    if (mButtonEvent.button != SDL_BUTTON_LEFT)
      return;

    SDL_Log("Mouse clicked");
  }
};
