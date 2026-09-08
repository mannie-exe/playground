#pragma once

#include <SDL3/SDL_surface.h>

#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/ui/Button.hpp>

class Cell : public Button {
public:
  Cell(const RectTransform transform, IInteractable &parent)
      : Button{transform,
               parent,
               {.baseColor = study_sdl3::minesweeper::config::buttonBaseColor,
                .hoverColor = study_sdl3::minesweeper::config::buttonHoverColor,
                .activeColor =
                    study_sdl3::minesweeper::config::buttonActiveColor}} {}
  ~Cell() = default;

  EventResult handleEvent(const SDL_Event &event) override {
    return Button::handleEvent(event);
  }

  void render(SDL_Surface &targetSurface) override {
    Button::render(targetSurface);
  }
};
