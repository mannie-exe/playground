#pragma once

#include <format>
#include <string>
#include <utility>

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

class DrawableSurface : public IDrawable {
  SDLResource<SDL_Surface, SDL_DestroySurface> _surface;

protected:
  bool _autoConvert;

public:
  DrawableSurface(SDL_Surface *surface, const bool autoConvert = false)
      : _surface{surface}, _autoConvert{autoConvert} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
  };

  DrawableSurface(SDLResource<SDL_Surface, SDL_DestroySurface> surface,
                  const bool autoConvert = false)
      : _surface{std::move(surface)}, _autoConvert{autoConvert} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
  }

  virtual void renderRect(SDL_Surface &targetSurface,
                          SDL_Rect *srcRect = nullptr,
                          SDL_Rect *dstRect = nullptr) {
    if (_autoConvert && targetSurface.format != _surface->format) {
      SDLResource<SDL_Surface, SDL_DestroySurface> converted{
          SDL_ConvertSurface(_surface.get(), targetSurface.format)};
      if (!converted)
        throwSDLError(
            std::format("DrawableSurface@{} Failed to convert surface format",
                        (void *)this));

      _surface.reset(converted.release());

      SDL_Log("%s", std::format("DrawableSurface@{} Surface format converted",
                                (void *)this)
                        .c_str());
    }

    if (!SDL_BlitSurface(_surface.get(), srcRect, &targetSurface, dstRect))
      throwSDLError(std::format("DrawableSurface@{} Failed to render "
                                "SDL_Surface@{} to SDL_Surface@{}",
                                (void *)this, (void *)_surface.get(),
                                (void *)&targetSurface));
  }

  virtual void render(SDL_Surface &targetSurface) override {
    renderRect(targetSurface);
  }

  SDL_Surface *getSurface() const { return _surface.get(); }

  void replaceSurface(
      SDLResource<SDL_Surface, SDL_DestroySurface> newSurface) noexcept {
    _surface.reset(newSurface.release());
  }

  DrawableSurface(DrawableSurface &&) noexcept = default;
  DrawableSurface &operator=(DrawableSurface &&) noexcept = default;

  DrawableSurface(const DrawableSurface &) = delete;
  DrawableSurface &operator=(const DrawableSurface &) = delete;
};
