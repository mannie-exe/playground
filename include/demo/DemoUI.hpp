#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL.h>

#include <interfaces/IDisplayObject.hpp>
#include <interfaces/IDrawable.hpp>
#include <interfaces/IInteractable.hpp>
#include <ui/Button.hpp>

class DemoUI : public IInteractable, public IDrawable {
  std::vector<std::unique_ptr<IDisplayObject>> _items;

public:
  DemoUI() {
    int row{15}, col{15};
    _items.reserve(row * col);

    for (int i{0}; i < row; i++) {
      for (int j{0}; j < col; j++) {
        _items.emplace_back(std::make_unique<Button>(
            rect(static_cast<float>(65 * i), static_cast<float>(65 * j),
                 static_cast<float>(50), static_cast<float>(50)),
            *this));
      }
    }
  }

  void render(SDL_Surface &targetSurface) override {
    for (const std::unique_ptr<IDisplayObject> &item : _items) {
      item->render(targetSurface);
    }
  }

  EventResult handleEvent(const SDL_Event &event) override {
    EventResult result{EventResult::Ignored};

    for (auto it = _items.rbegin(); it != _items.rend(); ++it) {
      result = combine(result, (*it)->handleEvent(event));
      if (isTerminal(result))
        break;
    }

    return result;
  }
};
