#pragma once

#include <string>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/minesweeper/MinesweeperEvents.hpp>
#include <study_sdl3/minesweeper/MinesweeperGrid.hpp>
#include <study_sdl3/platform/DisplayState.hpp>

class MinesweeperUI : public IDisplayObject {
  DisplayState _displayState;
  MinesweeperGrid _grid;

public:
  explicit MinesweeperUI(
      const DisplayState displayState, const RectTransform transform,
      const study_sdl3::minesweeper::MinesweeperEvents &events,
      const std::string &bombImagePath, Font &font)
      : IDisplayObject{transform}, _displayState{displayState},
        _grid{transform, events, bombImagePath, font} {}

  void setDisplayState(DisplayState displayState) {
    _displayState = displayState;
  }

  EventResult handleEvent(const SDL_Event &event) override {
    if (!isVisible())
      return EventResult::Ignored;
    return _grid.handleEvent(event);
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    _grid.render(targetSurface);
  }
};
