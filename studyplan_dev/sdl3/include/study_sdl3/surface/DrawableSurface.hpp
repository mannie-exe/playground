#pragma once

#include <format>
#include <string>

#include <study_sdl3/interfaces/IDrawable.hpp>

class DrawableSurface : public IDrawable {
  SDL_Surface *_surface;

protected:
  bool _autoConvert;

public:
  DrawableSurface(SDL_Surface *surface, const bool autoConvert = false)
      : _surface(surface), _autoConvert(autoConvert) {
    if (!surface) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format("DrawableSurface@{} received null SDL_Surface:\n  {}",
                          (void *)this, err);
        SDL_ClearError();
        throw err;
      }

      throw std::format("DrawableSurface@{} received null SDL_Surface",
                        (void *)this);
    }
  };

  virtual ~DrawableSurface() {
    if (!_surface)
      return;
    SDL_DestroySurface(_surface);
  }

  virtual void renderRect(SDL_Surface &targetSurface,
                          SDL_Rect *srcRect = nullptr,
                          SDL_Rect *dstRect = nullptr) {
    if (_autoConvert && targetSurface.format != _surface->format) {
      SDL_Surface *converted =
          SDL_ConvertSurface(_surface, targetSurface.format);
      if (!converted) {
        std::string err{SDL_GetError()};
        std::string exception;

        if (!err.empty()) {
          exception = std::format(
              "DrawableSurface@{} Failed to convert surface format:\n  {}",
              (void *)this, err);
          SDL_ClearError();
          throw exception;
        }

        throw std::format("DrawableSurface@{} Failed to convert surface format",
                          (void *)this);
      }

      SDL_DestroySurface(_surface);
      _surface = converted;

      SDL_Log("%s", std::format("DrawableSurface@{}: surface format converted",
                                (void *)this)
                        .c_str());
    }
    SDL_BlitSurface(_surface, srcRect, &targetSurface, dstRect);
  }

  virtual void render(SDL_Surface &targetSurface) override {
    renderRect(targetSurface);
  }

  SDL_Surface *getSurface() const { return _surface; }

  void replaceSurface(SDL_Surface *newSurface) {
    if (_surface == newSurface)
      return;

    if (!newSurface) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format("DrawableSurface::replaceSurface@{} received null "
                          "SDL_Surface:\n  {}",
                          (void *)this, err);
        SDL_ClearError();
        throw err;
      }

      throw std::format(
          "DrawableSurface::replaceSurface@{} received null SDL_Surface",
          (void *)this);
    }

    if (_surface) {
      SDL_DestroySurface(_surface);
    }

    _surface = newSurface;
  }
};
