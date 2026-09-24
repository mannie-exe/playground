#pragma once

#include <stdexcept>
#include <utility>

#include <rendering/PaintImage.hpp>
#include <support/SurfaceHandles.hpp>

namespace playground::sdl {

// Pixel contents must not be mutated while shared/cached as an image source.
class SurfacePaintImage final : public rendering::PaintImage {
  SurfaceHandle _surface;
  bool _premultiplied{};
  rendering::ColorEncoding _encoding;

public:
  explicit SurfacePaintImage(
      SurfaceHandle surface, bool premultiplied = false,
      rendering::ColorEncoding encoding = rendering::ColorEncoding::SRGB)
      : _surface{std::move(surface)}, _premultiplied{premultiplied},
        _encoding{encoding} {
    if (!_surface)
      throw std::invalid_argument("Paint image requires a surface");
    if (!rendering::isValid(encoding))
      throw std::invalid_argument("Unknown surface color encoding");
  }
  math::Size2 pixelSize() const noexcept override {
    return {static_cast<float>(_surface->w), static_cast<float>(_surface->h)};
  }
  const SurfaceHandle &surface() const noexcept { return _surface; }
  bool isPremultiplied() const noexcept { return _premultiplied; }
  rendering::AlphaMode alphaMode() const noexcept override {
    return _premultiplied ? rendering::AlphaMode::Premultiplied
                          : rendering::AlphaMode::Straight;
  }
  rendering::ColorEncoding colorEncoding() const noexcept override {
    return _encoding;
  }
};

inline rendering::PaintImageHandle makeSurfaceImage(
    SurfaceHandle surface, bool premultiplied = false,
    rendering::ColorEncoding encoding = rendering::ColorEncoding::SRGB) {
  return std::make_shared<SurfacePaintImage>(std::move(surface), premultiplied,
                                             encoding);
}

} // namespace playground::sdl
