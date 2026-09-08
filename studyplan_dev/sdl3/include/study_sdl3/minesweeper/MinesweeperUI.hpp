#pragma once

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/platform/DisplayState.hpp>

class MinesweeperUI : public IInteractable, public IDrawable {
  DisplayState _displayState;

public:
  MinesweeperUI(const DisplayState displayState)
      : _displayState{displayState} {}

  void setDisplayState(DisplayState displayState) {
    _displayState = displayState;
  }

  EventResult handleEvent(const SDL_Event &event) override {
    return EventResult::Ignored;
  }

  void render(SDL_Surface &targetSurface) override {}
};
