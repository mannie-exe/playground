#pragma once
#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/ui/Button.hpp>
#include <study_sdl3/ui/DisplayObject.hpp>

class UI : public IInteractable, public IDrawable {
  std::vector<std::unique_ptr<IDrawable>> _objects;
  std::vector<IInteractable *> _handlers;

public:
  UI() {
    int row{15}, col{15};
    _objects.reserve(row * col);

    for (int i{0}; i < row; i++) {
      for (int j{0}; j < col; j++) {
        std::unique_ptr<Button> button = std::make_unique<Button>(
            RectTransform{static_cast<float>(65 * i),
                          static_cast<float>(65 * j), static_cast<float>(50),
                          static_cast<float>(50)},
            *this);
        _handlers.emplace_back(button.get());
        _objects.emplace_back(std::move(button));
      }
    }
  }

  void render(SDL_Surface &targetSurface) override {
    for (const std::unique_ptr<IDrawable> &item : _objects) {
      item->render(targetSurface);
    }
  }

  EventResult handleEvent(const SDL_Event &event) override {
    EventResult result{EventResult::Ignored};
    bool handled{false};

    for (auto it = _handlers.rbegin(); it != _handlers.rend(); ++it) {
      result = (*it)->handleEvent(event);
      if (!handled)
        handled = result == EventResult::Handled ? true : handled;
      if (result == EventResult::Consumed)
        break;
    }

    return result == EventResult::Consumed ? result
           : handled                       ? EventResult::Handled
                                           : result;
  }
};
