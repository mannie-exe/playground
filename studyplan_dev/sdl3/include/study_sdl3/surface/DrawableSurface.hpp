#pragma once

#include <format>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_surface.h>

#include <study_sdl3/interfaces/IDrawable.hpp>
#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

using SurfaceResource = SDLResource<SDL_Surface, SDL_DestroySurface>;

struct SurfaceRenderProps {
  std::optional<SDL_Rect> srcRect;
  std::optional<SDL_Rect> dstRect;
  std::optional<SDL_ScaleMode> scaleMode;
};

class DrawableSurface : public IDrawable {
  SurfaceResource _surface;

protected:
  bool _autoConvert{false};

  SurfaceRenderProps _render{};

public:
  DrawableSurface() = default;
  explicit DrawableSurface(const bool autoConvert,
                           const SurfaceRenderProps render = {})
      : _autoConvert{autoConvert}, _render{render} {}

  DrawableSurface(SDL_Surface *surface, const bool autoConvert = false,
                  const SurfaceRenderProps render = {})
      : _surface{surface}, _autoConvert{autoConvert}, _render{render} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
  };

  DrawableSurface(SurfaceResource surface, const bool autoConvert = false)
      : _surface{std::move(surface)}, _autoConvert{autoConvert} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
  }

  std::optional<SDL_ScaleMode> getScaleMode() { return _render.scaleMode; }
  std::optional<SDL_Rect> getSrcRect() { return _render.srcRect; }
  std::optional<SDL_Rect> getDstRect() { return _render.dstRect; }

  void setScaleMode(SDL_ScaleMode scaleMode) {
    if (scaleMode == _render.scaleMode)
      return;
    _render.scaleMode = scaleMode;
  }

  void setSrcRect(SDL_Rect rect) {
    if (_render.srcRect) {
      if (rect.x == _render.srcRect->x && rect.y == _render.srcRect->y &&
          rect.w == _render.srcRect->w && rect.h == _render.srcRect->h)
        return;
    }
    _render.srcRect = std::make_optional(rect);
  }

  void setDstRect(SDL_Rect rect) {
    if (_render.dstRect) {
      if (rect.x == _render.dstRect->x && rect.y == _render.dstRect->y &&
          rect.w == _render.dstRect->w && rect.h == _render.dstRect->h)
        return;
    }
    _render.dstRect = std::make_optional(rect);
  }

  virtual void renderRect(SDL_Surface &targetSurface,
                          SurfaceRenderProps render) {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));

    if (_autoConvert && targetSurface.format != _surface->format) {
      SurfaceResource converted{
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

    if (render.scaleMode) {
      if (!SDL_BlitSurfaceScaled(
              _surface.get(), render.srcRect ? &*render.srcRect : nullptr,
              &targetSurface, render.dstRect ? &*render.dstRect : nullptr,
              *render.scaleMode)) {
        throwSDLError(std::format("DrawableSurface@{} Failed to render "
                                  "SDL_Surface@{} to SDL_Surface@{}",
                                  (void *)this, (void *)_surface.get(),
                                  (void *)&targetSurface));
      }
      return;
    }

    if (!SDL_BlitSurface(
            _surface.get(), render.srcRect ? &*render.srcRect : nullptr,
            &targetSurface, render.dstRect ? &*render.dstRect : nullptr))
      throwSDLError(std::format("DrawableSurface@{} Failed to render "
                                "SDL_Surface@{} to SDL_Surface@{}",
                                (void *)this, (void *)_surface.get(),
                                (void *)&targetSurface));
  }

  virtual void render(SDL_Surface &targetSurface) override {
    renderRect(targetSurface, _render);
  }

  SDL_Surface *getSurface() const { return _surface.get(); }

  DrawableSurface(DrawableSurface &&) noexcept = default;
  DrawableSurface &operator=(DrawableSurface &&) noexcept = default;

  DrawableSurface(const DrawableSurface &) = delete;
  DrawableSurface &operator=(const DrawableSurface &) = delete;

protected:
  void replaceSurface(SurfaceResource newSurface) noexcept {
    _surface.reset(newSurface.release());
  }
};
