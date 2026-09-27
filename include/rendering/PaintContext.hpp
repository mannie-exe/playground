#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <math/Shapes2D.hpp>
#include <rendering/ImagePreparer.hpp>
#include <rendering/PaintImage.hpp>
#include <rendering/PathPaint.hpp>
#include <rendering/ResourceDomain.hpp>
#include <rendering/TextImagePreparer.hpp>

namespace playground::rendering {

// Borrowed for synchronous painting/command recording; never retain in nodes
// or asynchronous callbacks. Backends control the target and restore drawing
// state through save/restore, not pixels. Calls need not finish GPU execution.
class PaintContext {
public:
  virtual ~PaintContext() = default;

  // Null selects identity preparation (software sources / recording contexts).
  virtual rendering::ImagePreparer *imagePreparer() noexcept { return nullptr; }

  // Realized layer images cannot cross backend/device lifetimes.
  virtual ResourceDomainId resourceDomain() const noexcept {
    return ResourceDomainId::cpu();
  }

  virtual TextImagePreparer *textPreparer() noexcept { return nullptr; }

  virtual std::size_t captureBytesPerPixel() const noexcept { return 4; }

  virtual void save() = 0;
  virtual void restore() noexcept = 0;
  virtual void translate(math::Vec2f offset) = 0;
  virtual void clip(math::Rect rectangle) = 0;
  virtual void fill(math::Rect rectangle, math::ColorRGBA8 color) = 0;

  virtual void drawPath(const math::Path2D &, const PathPaint &) {
    throw std::logic_error("Paint backend does not support vector paths");
  }

  virtual void clipRounded(math::RoundedRect) {
    throw std::logic_error("Paint backend does not support rounded clips");
  }

  virtual void fillRounded(math::RoundedRect, math::ColorRGBA8) {
    throw std::logic_error("Paint backend does not support rounded rectangles");
  }

  virtual void strokeRounded(math::RoundedRect, math::Insets,
                             math::ColorRGBA8) {
    throw std::logic_error("Paint backend does not support rounded borders");
  }

  virtual void paintRoundedBox(math::RoundedRect, math::Insets,
                               math::ColorRGBA8,
                               std::optional<math::ColorRGBA8>) {
    throw std::logic_error(
        "Paint backend does not support joined rounded boxes");
  }

  // Source coordinates are image pixels; destination coordinates are local
  // logical units. Deferred backends retain resources needed by recorded work.
  virtual void drawImage(const rendering::PaintImageHandle &image,
                         math::Rect sourcePixels, math::Rect destination,
                         rendering::ImagePaint appearance) {
    throw std::logic_error("Paint backend does not support images");
  }

  virtual void transform(math::Transform2D value) {
    if (value.a != 1 || value.b != 0 || value.c != 0 || value.d != 1)
      throw std::logic_error("Paint backend does not support this transform");
    translate({value.tx, value.ty});
  }

  virtual math::Vec2f pixelScale() const noexcept { return {1, 1}; }

  virtual void beginLayer(math::Rect, float) {
    throw std::logic_error("Paint backend does not support group compositing");
  }

  virtual void endLayer() {
    throw std::logic_error("Paint backend does not support group compositing");
  }

  virtual void cancelLayer() noexcept {}

  virtual std::shared_ptr<const rendering::PaintImage>
  capture(math::Rect bounds, math::Vec2f scale,
          const std::function<void(PaintContext &)> &draw) {
    throw std::logic_error(
        "Paint backend does not support retained raster layers");
  }
};

class LayerScope {
  PaintContext &_context;
  bool _active{true};

public:
  LayerScope(PaintContext &context, math::Rect bounds, float opacity)
      : _context{context} {
    _context.beginLayer(bounds, opacity);
  }

  ~LayerScope() {
    if (_active)
      _context.cancelLayer();
  }

  LayerScope(const LayerScope &) = delete;
  LayerScope &operator=(const LayerScope &) = delete;

  void finish() {
    _context.endLayer();
    _active = false;
  }
};

class PaintScope {
  PaintContext &_context;

public:
  explicit PaintScope(PaintContext &context) : _context{context} {
    _context.save();
  }

  ~PaintScope() { _context.restore(); }

  PaintScope(const PaintScope &) = delete;
  PaintScope &operator=(const PaintScope &) = delete;
};

} // namespace playground::rendering
