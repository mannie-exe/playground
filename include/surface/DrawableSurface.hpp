#pragma once

#include <algorithm>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

#include <SDL3/SDL_surface.h>

#include <support/SurfaceHandles.hpp>
#include <support/SDLError.hpp>
#include <support/SDLPrimitives.hpp>
#include <support/SDLResource.hpp>

using SurfaceResource = SDLResource<SDL_Surface, SDL_DestroySurface>;

enum class SurfaceFitMode {
  Stretch,
  Contain,
  Cover,
  Shrink,
};

constexpr std::string_view toString(SurfaceFitMode method) {
  switch (method) {
  case SurfaceFitMode::Stretch:
    return "Stretch";
  case SurfaceFitMode::Contain:
    return "Contain";
  case SurfaceFitMode::Cover:
    return "Cover";
  case SurfaceFitMode::Shrink:
    return "Shrink";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<SurfaceFitMode> : std::formatter<std::string_view> {
  auto format(SurfaceFitMode method, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(method), ctx);
  }
};

struct SurfaceBlitProps {
  std::optional<SDL_Rect> srcRect;
  SurfaceFitMode fitMode{SurfaceFitMode::Stretch};
};

struct SurfaceAppearance {
  SDL_Color colorMod{255, 255, 255, 255};
  Uint8 alphaMod{255};
  SDL_BlendMode blendMode{SDL_BLENDMODE_BLEND};
};

struct SurfaceAppearancePatch {
  std::optional<SDL_Color> colorMod;
  std::optional<Uint8> alphaMod;
  std::optional<SDL_BlendMode> blendMode;
};

struct SurfaceBlitPatch {
  std::optional<std::optional<SDL_Rect>> srcRect;
  std::optional<SurfaceFitMode> fitMode;
};

struct SurfaceSamplingProps {
  SDL_ScaleMode scaleMode{SDL_SCALEMODE_LINEAR};
};

struct SurfaceResolvedBlit {
  SDL_Rect srcRect;
  SDL_Rect dstRect;
};

struct SurfaceRenderProps {
  SurfaceBlitProps blit;
  SurfaceSamplingProps sampling;
  SurfaceAppearance appearance;
};

struct SurfaceRenderPatch {
  std::optional<SurfaceAppearancePatch> appearance;
  std::optional<SurfaceBlitPatch> blit;
  std::optional<SDL_ScaleMode> samplingMode;
};

class DrawableSurface {
  SurfaceResource _surface;

protected:
  bool _autoConvert;
  SurfaceRenderProps _render;

public:
  DrawableSurface(const SurfaceRenderProps render = {},
                  const bool autoConvert = true)
      : _render{render}, _autoConvert{autoConvert} {}

  explicit DrawableSurface(SDL_Surface *surface,
                           const SurfaceRenderProps render = {},
                           const bool autoConvert = true)
      : _surface{surface}, _render{render}, _autoConvert{autoConvert} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
    applyAppearance(_render.appearance);
  };

  explicit DrawableSurface(SurfaceResource surface,
                           const SurfaceRenderProps render = {},
                           const bool autoConvert = true)
      : _surface{std::move(surface)}, _render{render},
        _autoConvert{autoConvert} {
    if (!_surface)
      throwSDLError(std::format("DrawableSurface@{} Received null SDL_Surface",
                                (void *)this));
    applyAppearance(_render.appearance);
  }

  explicit DrawableSurface(const SurfaceHandle &surface,
                           const SurfaceRenderProps render = {},
                           const bool autoConvert = false)
      : DrawableSurface{
            SurfaceResource{surface ? SDL_DuplicateSurface(surface.get())
                                    : nullptr},
            render, autoConvert} {}

  SDL_Rect getSize() const {
    if (!_surface)
      return SDL_Rect{0, 0, 0, 0};
    return SDL_Rect{0, 0, _surface->w, _surface->h};
  }

  SurfaceRenderProps getRenderProps() const { return _render; }
  SurfaceBlitProps getBlitProps() const { return _render.blit; }
  SurfaceSamplingProps getSamplingProps() const { return _render.sampling; }

  void setRenderProps(SurfaceRenderProps render) {
    applyAppearance(render.appearance);
    _render = render;
  }

  void setBlitProps(SurfaceBlitProps blit) { _render.blit = blit; }

  void setSamplingProps(SurfaceSamplingProps sampling) {
    _render.sampling = sampling;
  }

  void setAppearance(SurfaceAppearance appearance) {
    applyAppearance(appearance);
    _render.appearance = appearance;
  }

  SurfaceAppearance getAppearance() const { return _render.appearance; }

  void setColorMod(SDL_Color colorMod) {
    SurfaceAppearance appearance{_render.appearance};
    appearance.colorMod = colorMod;
    setAppearance(appearance);
  }

  SDL_Color getColorMod() const { return _render.appearance.colorMod; }

  void setAlphaMod(Uint8 alphaMod) {
    SurfaceAppearance appearance{_render.appearance};
    appearance.alphaMod = alphaMod;
    setAppearance(appearance);
  }

  Uint8 getAlphaMod() const { return _render.appearance.alphaMod; }

  void setBlendMode(SDL_BlendMode blendMode) {
    SurfaceAppearance appearance{_render.appearance};
    appearance.blendMode = blendMode;
    setAppearance(appearance);
  }

  SDL_BlendMode getBlendMode() const { return _render.appearance.blendMode; }

  void applyRenderPatch(const SurfaceRenderPatch &patch) {
    SurfaceRenderProps next{_render};

    if (patch.appearance) {
      const SurfaceAppearancePatch &appearance{*patch.appearance};
      if (appearance.colorMod)
        next.appearance.colorMod = *appearance.colorMod;
      if (appearance.alphaMod)
        next.appearance.alphaMod = *appearance.alphaMod;
      if (appearance.blendMode)
        next.appearance.blendMode = *appearance.blendMode;
    }

    if (patch.blit) {
      const SurfaceBlitPatch &blit{*patch.blit};
      if (blit.srcRect)
        next.blit.srcRect = *blit.srcRect;
      if (blit.fitMode)
        next.blit.fitMode = *blit.fitMode;
    }

    if (patch.samplingMode)
      next.sampling.scaleMode = *patch.samplingMode;

    setRenderProps(next);
  }

  void setSrcRect(SDL_Rect rect) {
    if (_render.blit.srcRect) {
      if (rect == *_render.blit.srcRect)
        return;
    }
    _render.blit.srcRect = rect;
  }

  void clearSrcRect() { _render.blit.srcRect.reset(); }

  void setSamplingMode(SDL_ScaleMode samplingMode) {
    if (samplingMode == _render.sampling.scaleMode)
      return;
    _render.sampling.scaleMode = samplingMode;
  }

  void setFitMode(SurfaceFitMode fitMode) {
    if (fitMode == _render.blit.fitMode)
      return;
    _render.blit.fitMode = fitMode;
  }

  virtual void renderInto(SDL_Surface &targetSurface,
                          const RectTransform destination,
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
    }

    SurfaceResolvedBlit blit = resolveBlit(getSize(), destination, render.blit);

    if (!SDL_BlitSurfaceScaled(_surface.get(), &blit.srcRect, &targetSurface,
                               &blit.dstRect, render.sampling.scaleMode)) {
      throwSDLError(std::format("DrawableSurface@{} Failed to render "
                                "SDL_Surface@{} to SDL_Surface@{}",
                                (void *)this, (void *)_surface.get(),
                                (void *)&targetSurface));
    }
  }

  virtual void renderInto(SDL_Surface &targetSurface,
                          const RectTransform destination) {
    renderInto(targetSurface, destination, _render);
  }

  virtual void render(SDL_Surface &targetSurface) {
    const SDL_Rect size{getSize()};
    renderInto(targetSurface,
               rect(0.0f, 0.0f, static_cast<float>(size.w),
                    static_cast<float>(size.h)),
               _render);
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

  static SurfaceResolvedBlit resolveBlit(const SDL_Rect surfaceRect,
                                         const RectTransform destination,
                                         const SurfaceBlitProps blit) {
    const SDL_Rect srcRect = blit.srcRect.value_or(surfaceRect);
    const SDL_Rect dstRect = destination.toSDL();

    SurfaceResolvedBlit resolved{.srcRect = srcRect, .dstRect = dstRect};

    if (blit.fitMode == SurfaceFitMode::Stretch)
      return resolved;

    if (srcRect.w <= 0 || srcRect.h <= 0 || dstRect.w <= 0 || dstRect.h <= 0)
      return resolved;

    if (blit.fitMode == SurfaceFitMode::Contain) {
      float scale = std::min(dstRect.w / static_cast<float>(srcRect.w),
                             dstRect.h / static_cast<float>(srcRect.h));

      int w = srcRect.w * scale;
      int h = srcRect.h * scale;

      resolved.dstRect.x = dstRect.x + (dstRect.w - w) / 2;
      resolved.dstRect.y = dstRect.y + (dstRect.h - h) / 2;
      resolved.dstRect.w = w;
      resolved.dstRect.h = h;

      return resolved;
    }

    if (blit.fitMode == SurfaceFitMode::Cover) {
      const float srcRatio{srcRect.w / static_cast<float>(srcRect.h)};
      const float dstRatio{dstRect.w / static_cast<float>(dstRect.h)};

      if (srcRatio > dstRatio) {
        int newW = srcRect.h * dstRatio;
        resolved.srcRect.x = srcRect.x + (srcRect.w - newW) / 2;
        resolved.srcRect.w = newW;
      } else {
        int newH = srcRect.w / dstRatio;
        resolved.srcRect.y = srcRect.y + (srcRect.h - newH) / 2;
        resolved.srcRect.h = newH;
      }

      return resolved;
    }

    if (blit.fitMode == SurfaceFitMode::Shrink) {
      float containScale = std::min(dstRect.w / static_cast<float>(srcRect.w),
                                    dstRect.h / static_cast<float>(srcRect.h));
      float scale = std::min(containScale, 1.0f);

      int w = srcRect.w * scale;
      int h = srcRect.h * scale;

      resolved.dstRect.x = dstRect.x + (dstRect.w - w) / 2;
      resolved.dstRect.y = dstRect.y + (dstRect.h - h) / 2;
      resolved.dstRect.w = w;
      resolved.dstRect.h = h;

      return resolved;
    }

    return resolved;
  }

  void applyAppearance(const SurfaceAppearance &appearance) {
    if (!_surface)
      return;

    if (!SDL_SetSurfaceColorMod(_surface.get(), appearance.colorMod.r,
                                appearance.colorMod.g, appearance.colorMod.b))
      throwSDLError(std::format("DrawableSurface@{} Failed to set color "
                                "modulation",
                                (void *)this));

    if (!SDL_SetSurfaceAlphaMod(_surface.get(), appearance.alphaMod))
      throwSDLError(std::format("DrawableSurface@{} Failed to set alpha "
                                "modulation",
                                (void *)this));

    if (!SDL_SetSurfaceBlendMode(_surface.get(), appearance.blendMode))
      throwSDLError(std::format("DrawableSurface@{} Failed to set blend "
                                "mode",
                                (void *)this));
  }
};
