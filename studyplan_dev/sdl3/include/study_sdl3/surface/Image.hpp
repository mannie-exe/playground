#pragma once

#include <string>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <study_sdl3/surface/DrawableSurface.hpp>

class Image : public DrawableSurface {
public:
  explicit Image(const std::string &filePath, bool autoConvert = true)
      : DrawableSurface(IMG_Load(filePath.c_str()), autoConvert) {
    SDL_Log("%s",
            std::format("Image@{} loaded: {}", (void *)this, filePath).c_str());
  }

  Image(Image &&) noexcept = default;
  Image &operator=(Image &&) noexcept = default;

  Image(const Image &) = delete;
  Image &operator=(const Image &) = delete;
};
