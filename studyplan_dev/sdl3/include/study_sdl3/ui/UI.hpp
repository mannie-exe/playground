#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <study_sdl3/ui/Button.hpp>
#include <study_sdl3/ui/DisplayObject.hpp>

class UI : public DisplayObject {
  std::vector<std::unique_ptr<DisplayObject>> _drawables;

public:
  UI() {
    int row{15}, col{15};
    _drawables.reserve(row * col);

    for (int i{0}; i < row; i++) {
      for (int j{0}; j < col; j++) {
        _drawables.emplace_back(
            std::make_unique<Button>(SDL_Rect{65 * i, 65 * j, 50, 50}, *this));
      }
    }
  }

  bool isPointInObject(float x, float y) const override { return true; }

  void render(SDL_Surface &targetSurface) override {
    for (const std::unique_ptr<DisplayObject> &item : _drawables) {
      item->render(targetSurface);
    }
  }

  void handleEvent(const SDL_Event &event) override {
    DisplayObject::handleEvent(event);

    for (std::unique_ptr<DisplayObject> &item : _drawables) {
      item->handleEvent(event);
    }
  }
};
