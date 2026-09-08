#pragma once

#include <SDL3/SDL_surface.h>

#include <study_sdl3/surface/DrawableSurface.hpp>
#include <study_sdl3/surface/ImageSurface.hpp>
#include <study_sdl3/ui/IDisplayObject.hpp>

class DisplayImage : public IDisplayObject {
  ImageSurface _image;

public:
  explicit DisplayImage(const std::string &filePath,
                        const SurfaceRenderProps render = {},
                        const RectTransform transform = {})
      : IDisplayObject{transform}, _image{filePath, render} {}

  void setTransform(const RectTransform &transform) override {
    IDisplayObject::setTransform(transform);
  }

  void render(SDL_Surface &targetSurface) override {
    _image.renderInto(targetSurface, _transform);
  }
};
