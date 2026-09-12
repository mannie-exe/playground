#pragma once

#include <format>
#include <string>
#include <utility>

#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

#include <support/SDLResource.hpp>
#include <surface/DrawableSurface.hpp>

class VectorSurface : public DrawableSurface {
  using StreamResource = SDLResource<SDL_IOStream, SDL_CloseIO>;

  std::string _filePath;
  Vec2i _rasterSize{};

  static SurfaceResource loadSVG(const std::string &filePath,
                                 const Vec2i rasterSize) {
    StreamResource stream{SDL_IOFromFile(filePath.c_str(), "rb")};
    if (!stream)
      throwSDLError(
          std::format("VectorSurface Failed to open SVG: {}", filePath));

    SDL_Surface *surface{};
    if (hasArea(rasterSize))
      surface = IMG_LoadSizedSVG_IO(stream.get(), rasterSize.x, rasterSize.y);
    else
      surface = IMG_LoadSVG_IO(stream.get());

    if (!surface)
      throwSDLError(
          std::format("VectorSurface Failed to load SVG: {}", filePath));

    return SurfaceResource{surface};
  }

public:
  explicit VectorSurface(const std::string &filePath,
                         const Vec2i rasterSize = {},
                         const SurfaceRenderProps renderProps = {})
      : DrawableSurface{loadSVG(filePath, rasterSize), renderProps, false},
        _filePath{filePath}, _rasterSize{rasterSize} {}

  explicit VectorSurface(const SurfaceHandle &surface, const Vec2i rasterSize,
                         const std::string &filePath = {},
                         const SurfaceRenderProps renderProps = {})
      : DrawableSurface{surface, renderProps, false}, _filePath{filePath},
        _rasterSize{rasterSize} {}

  const std::string &getFilePath() const { return _filePath; }
  Vec2i getRasterSize() const { return _rasterSize; }

  void setRasterSize(const Vec2i rasterSize) {
    if (rasterSize == _rasterSize)
      return;

    SurfaceResource next{loadSVG(_filePath, rasterSize)};
    replaceSurface(std::move(next));
    applyAppearance(_render.appearance);
    _rasterSize = rasterSize;
  }

  VectorSurface(VectorSurface &&) noexcept = default;
  VectorSurface &operator=(VectorSurface &&) noexcept = default;

  VectorSurface(const VectorSurface &) = delete;
  VectorSurface &operator=(const VectorSurface &) = delete;
};
