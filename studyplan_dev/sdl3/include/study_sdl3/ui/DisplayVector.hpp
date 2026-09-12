#pragma once

#include <algorithm>
#include <string>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/surface/VectorSurface.hpp>

class DisplayVector : public IDisplayObject {
  VectorSurface _vector;

  static Vec2i rasterSize(const RectTransform transform) {
    return Vec2i{std::max(0, static_cast<int>(transform.size.x)),
                 std::max(0, static_cast<int>(transform.size.y))};
  }

public:
  explicit DisplayVector(const std::string &filePath,
                         const RectTransform transform = {},
                         const SurfaceRenderProps render = {},
                         const bool visible = true)
      : IDisplayObject{transform, visible},
        _vector{filePath, rasterSize(transform), render} {}

  explicit DisplayVector(const SurfaceHandle &surface,
                         const RectTransform transform = {},
                         const SurfaceRenderProps render = {},
                         const bool visible = true)
      : IDisplayObject{transform, visible},
        _vector{surface, rasterSize(transform), {}, render} {}

  void setTransform(const RectTransform &transform) override {
    _vector.setRasterSize(rasterSize(transform));
    IDisplayObject::setTransform(transform);
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    _vector.renderInto(targetSurface, _transform);
  }
};
