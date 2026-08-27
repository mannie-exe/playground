#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/ui/DisplayObject.hpp>

class Rectangle : public DisplayObject {
  SDL_Rect _rect;
  SDL_Color _color;
  SDL_Color _hoverColor;

public:
  Rectangle(const SDL_Rect &rect,
            const SDL_Color &color = SDL_Color{192, 0, 30},
            const SDL_Color &hoverColor = SDL_Color{220, 30, 110})
      : _rect{rect}, _color{color}, _hoverColor(hoverColor) {}

  void setColor(const SDL_Color &newColor) { _color = newColor; }
  void setHoverColor(const SDL_Color &newColor) { _hoverColor = newColor; }

  SDL_Color getColor() const { return _color; }
  SDL_Color getHoverColor() const { return _hoverColor; }

  bool isPointInObject(float x, float y) const override {
    return (x >= _rect.x && x < _rect.x + _rect.w && y >= _rect.y &&
            y < _rect.y + _rect.h);
  }

  void render(SDL_Surface &targetSurface) override {
    const auto *pixelFormat = SDL_GetPixelFormatDetails(targetSurface.format);
    const SDL_Color &color = _hover ? _hoverColor : _color;

    SDL_FillSurfaceRect(
        &targetSurface, &_rect,
        SDL_MapRGB(pixelFormat, nullptr, color.r, color.g, color.b));
  }

  void handleEvent(const SDL_Event &event) override {
    DisplayObject::handleEvent(event);

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      bool isHovering = isPointInObject(event.motion.x, event.motion.y);

      if (isHovering && !_hover) {
        _hover = isHovering;
        onMouseEnter();
      }

      if (!isHovering && _hover) {
        _hover = isHovering;
        onMouseExit();
      }
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
      if (_hover) {
        _hover = false;
        onMouseExit();
      }
    }

    if (event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) {
      float x, y;
      SDL_GetMouseState(&x, &y);
      bool isHovering = isPointInObject(x, y);

      if (isHovering && !_hover) {
        _hover = isHovering;
        onMouseEnter();
      }
    }
  }
};
