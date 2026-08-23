#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <DisplayObject.hpp>
#include <Rectangle.hpp>

class UI : public DisplayObject {
public:
  UI() {
    int row{15}, col{15};
    _drawables.reserve(row * col);

    for (int i{0}; i < row; i++) {
      for (int j{0}; j < col; j++) {
        _drawables.emplace_back(
            std::make_unique<Rectangle>(SDL_Rect{65 * i, 65 * j, 50, 50}));
      }
    }
  }

  bool isPointInObject(int x, int y) const override {
    // TODO
    return true;
  }

  void render(SDL_Surface &surface) const override {
    for (const std::unique_ptr<DisplayObject> &item : _drawables) {
      item->render(surface);
    }
  }

  void handleEvent(const SDL_Event &event) override {
    DisplayObject::handleEvent(event);

    for (std::unique_ptr<DisplayObject> &item : _drawables) {
      item->handleEvent(event);
    }
  }

private:
  std::vector<std::unique_ptr<DisplayObject>> _drawables;
};
