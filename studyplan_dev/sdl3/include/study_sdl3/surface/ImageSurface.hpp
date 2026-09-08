#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <study_sdl3/surface/DrawableSurface.hpp>

class ImageSurface : public DrawableSurface {
  std::string _filePath;

public:
  explicit ImageSurface(const std::string &filePath,
                        const SurfaceRenderProps renderProps = {})
      : DrawableSurface(
            requireSDL(IMG_Load(filePath.c_str()),
                       std::format("ImageSurface@{} Failed to load: {}",
                                   (void *)this, filePath)),
            renderProps, true),
        _filePath{filePath} {
    SDL_Log("%s",
            std::format("ImageSurface@{} Loaded: {}", (void *)this, filePath)
                .c_str());
  }

  const std::string &getFilePath() const { return _filePath; }

  ImageSurface(ImageSurface &&) noexcept = default;
  ImageSurface &operator=(ImageSurface &&) noexcept = default;

  ImageSurface(const ImageSurface &) = delete;
  ImageSurface &operator=(const ImageSurface &) = delete;
};
