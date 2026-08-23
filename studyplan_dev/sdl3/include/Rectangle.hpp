#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <DisplayObject.hpp>
#include <IDrawable.hpp>
#include <IInteractable.hpp>

class Rectangle : public DisplayObject {
public:
  Rectangle(const SDL_Rect &rect,
            const SDL_Color &color = SDL_Color{192, 0, 30},
            const SDL_Color &hoverColor = SDL_Color{220, 30, 110})
      : _rect{rect}, _color{color}, _hoverColor(hoverColor) {}

  void setColor(const SDL_Color &newColor) { _color = newColor; }
  void setHoverColor(const SDL_Color &newColor) { _hoverColor = newColor; }

  SDL_Color getColor() const { return _color; }
  SDL_Color getHoverColor() const { return _hoverColor; }

  bool isPointInObject(int x, int y) const override {
    return (x >= _rect.x && x < _rect.x + _rect.w && y >= _rect.y &&
            y < _rect.y + _rect.h);
  }

  void render(SDL_Surface &surface) const override {
    const auto *pixelFormat = SDL_GetPixelFormatDetails(surface.format);
    const SDL_Color &color = DisplayObject::hover ? _hoverColor : _color;

    SDL_FillSurfaceRect(
        &surface, &_rect,
        SDL_MapRGB(pixelFormat, nullptr, color.r, color.g, color.b));
  }

  void handleEvent(const SDL_Event &event) override {
    DisplayObject::handleEvent(event);

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      DisplayObject::hover = isPointInObject(event.motion.x, event.motion.y);
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
      DisplayObject::hover = false;
    }
  }

private:
  SDL_Rect _rect;
  SDL_Color _color;
  SDL_Color _hoverColor;
};
