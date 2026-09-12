#pragma once

#include <optional>

#include <SDL3/SDL.h>
#include <SDL3/SDL_rect.h>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>

struct RectangleStyle {
  SDL_Color color;
};

struct RectangleStylePatch {
  std::optional<SDL_Color> color;
};

class Rectangle : public IDisplayObject {
  RectangleStyle _style;

public:
  Rectangle(const RectTransform rect,
            const RectangleStyle style = {.color = SDL_Color{192, 0, 30, 255}},
            const bool visible = true)
      : IDisplayObject{rect, visible}, _style{style} {}

  SDL_Color getColor() const { return _style.color; }

  RectangleStyle getStyle() const { return _style; }

  void setColor(const SDL_Color &newColor) {
    if (_style.color == newColor)
      return;
    _style.color = newColor;
  }

  void setStyle(const RectangleStyle &style) {
    if (_style.color == style.color)
      return;
    _style = style;
  }

  void applyStylePatch(const RectangleStylePatch &patch) {
    if (patch.color)
      setColor(*patch.color);
  }

  void render(SDL_Surface &targetSurface, SDL_Color color) {
    const auto *pixelFormat = SDL_GetPixelFormatDetails(targetSurface.format);
    const SDL_Rect rect = _transform.toSDL();

    SDL_FillSurfaceRect(
        &targetSurface, &rect,
        SDL_MapRGB(pixelFormat, nullptr, color.r, color.g, color.b));
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    render(targetSurface, _style.color);
  }
};
