#pragma once

#include <SDL3/SDL_surface.h>

#include <interfaces/IDisplayObject.hpp>
#include <surface/DrawableSurface.hpp>
#include <surface/ImageSurface.hpp>

class DisplayImage : public IDisplayObject {
  ImageSurface _image;

public:
  explicit DisplayImage(const std::string &filePath,
                        const RectTransform transform = {},
                        const SurfaceRenderProps render = {},
                        const bool visible = true)
      : IDisplayObject{transform, visible}, _image{filePath, render} {}

  explicit DisplayImage(const SurfaceHandle &surface,
                        const RectTransform transform = {},
                        const SurfaceRenderProps render = {},
                        const bool visible = true)
      : IDisplayObject{transform, visible}, _image{surface, {}, render} {}

  void setTransform(const RectTransform &transform) override {
    IDisplayObject::setTransform(transform);
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    _image.renderInto(targetSurface, _transform);
  }
};
