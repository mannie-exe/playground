#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <study_sdl3/surface/DrawableSurface.hpp>

class ImageSurface : public DrawableSurface {
  std::string _filePath;

public:
  explicit ImageSurface(const std::string &filePath,
                        const SurfaceRenderProps renderProps = {},
                        const bool autoConvert = false)
      : DrawableSurface(
            requireSDL(IMG_Load(filePath.c_str()),
                       std::format("ImageSurface@{} Failed to load: {}",
                                   (void *)this, filePath)),
            renderProps, autoConvert),
        _filePath{filePath} {
    if (!SDL_SetSurfaceBlendMode(getSurface(), SDL_BLENDMODE_BLEND))
      throwSDLError(std::format("ImageSurface@{} Failed to set blend mode: {}",
                                (void *)this, filePath));

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
