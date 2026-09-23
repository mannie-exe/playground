#pragma once

#include <stdexcept>
#include <support/SurfaceHandles.hpp>
#include <ui/PaintImage.hpp>
#include <utility>

namespace playground::sdl {

// Pixel contents must not be mutated while shared/cached as an image source.
class SurfacePaintImage final : public ui::PaintImage {
  SurfaceHandle _surface;
  bool _premultiplied{};

public:
  explicit SurfacePaintImage(SurfaceHandle surface, bool premultiplied = false)
      : _surface{std::move(surface)}, _premultiplied{premultiplied} {
    if (!_surface)
      throw std::invalid_argument("Paint image requires a surface");
  }
  math::Size2 pixelSize() const noexcept override {
    return {static_cast<float>(_surface->w), static_cast<float>(_surface->h)};
  }
  const SurfaceHandle &surface() const noexcept { return _surface; }
  bool isPremultiplied() const noexcept { return _premultiplied; }
};

inline ui::PaintImageHandle makeSurfaceImage(SurfaceHandle surface,
                                             bool premultiplied = false) {
  return std::make_shared<SurfacePaintImage>(std::move(surface), premultiplied);
}

} // namespace playground::sdl
