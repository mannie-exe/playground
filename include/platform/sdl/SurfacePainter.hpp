#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_surface.h>

#include <platform/sdl/SurfacePaintImage.hpp>
#include <rendering/PaintContext.hpp>
#include <rendering/PaintImage.hpp>
#include <support/SDLResource.hpp>
#include <support/SurfaceHandles.hpp>

namespace playground::sdl {

struct SurfacePainterProps {
  math::Vec2f pixelScale{1, 1};
  std::size_t layerByteBudget{64 * 1024 * 1024};
  std::shared_ptr<rendering::ResourceLedger> resources{
      rendering::defaultResourceLedger()};
};

// Synchronous, UI-thread-only backend. Source modulation is scoped and
// restored; neither this painter nor its shared SDL resources support
// concurrent mutation.
class SurfacePainter final : public rendering::PaintContext {
  using OwnedSurface = SurfaceHandle;

  struct Mask {
    math::RoundedRect shape;
    math::Transform2D inverse;
  };

  struct State {
    math::Transform2D transform;
    math::Rect clip;
    std::vector<Mask> masks;
  };

  struct Layer {
    OwnedSurface surface;
    SDL_Surface *parent;
    math::Vec2i parentOrigin;
    math::Vec2i origin;
    State state;
    float opacity;
    std::size_t bytes;
    std::size_t saveDepth;
    bool restored{};
  };

  struct ModulationGuard;

  SurfacePainterProps _props;

  SDL_Surface &_root;
  SDL_Surface *_target;
  SDL_Rect _originalClip;
  math::Vec2i _origin{};
  State _state;
  std::vector<State> _states;
  std::vector<Layer> _layers;
  std::size_t _layerBytes{};
  OwnedSurface _colorPixel;

  static void require(bool result);

  static void validate(math::Rect bounds);

  SDL_Rect targetRect(math::Rect global) const;

  math::Rect mapped(math::Rect local) const;

  bool applyClip();

  void restoreLayer() noexcept;

  bool simple() const noexcept {
    const auto &t = _state.transform;
    return t.b == 0 && t.c == 0 && t.a > 0 && t.d > 0 && _state.masks.empty();
  }

  struct Pixel {
    double r{}, g{}, b{}, a{};

    Pixel operator*(double value) const {
      return {r * value, g * value, b * value, a * value};
    }

    Pixel &operator+=(Pixel value) {
      r += value.r;
      g += value.g;
      b += value.b;
      a += value.a;
      return *this;
    }
  };

  static Pixel read(SDL_Surface &surface, int x, int y, bool premultiplied);

  template <typename Sample> void raster(math::Rect local, Sample sample);

  static Pixel solid(math::ColorRGBA8 color);

  void rasterImage(const SurfacePaintImage &image, math::Rect source,
                   math::Rect destination, rendering::ImagePaint appearance);

public:
  explicit SurfacePainter(SDL_Surface &target,
                          math::Vec2f logicalToPixel = {1, 1})
      : SurfacePainter{target, SurfacePainterProps{logicalToPixel}} {}

  SurfacePainter(SDL_Surface &target, SurfacePainterProps props);

  ~SurfacePainter() override;

  SurfacePainter(const SurfacePainter &) = delete;
  SurfacePainter &operator=(const SurfacePainter &) = delete;

  void save() override { _states.push_back(_state); }

  void restore() noexcept override;

  void translate(math::Vec2f offset) override {
    transform(math::Transform2D::translation(offset));
  }

  void transform(math::Transform2D value) override;

  math::Vec2f pixelScale() const noexcept override {
    return {std::hypot(_state.transform.a, _state.transform.b),
            std::hypot(_state.transform.c, _state.transform.d)};
  }

  void clip(math::Rect rectangle) override;

  void clipRounded(math::RoundedRect shape) override;

  void fillRounded(math::RoundedRect shape, math::ColorRGBA8 color) override;

  void strokeRounded(math::RoundedRect shape, math::Insets widths,
                     math::ColorRGBA8 color) override;

  void paintRoundedBox(math::RoundedRect shape, math::Insets widths,
                       math::ColorRGBA8 fill,
                       std::optional<math::ColorRGBA8> border) override;

  void fill(math::Rect rectangle, math::ColorRGBA8 color) override;
  void drawPath(const math::Path2D &, const rendering::PathPaint &) override;

  void drawImage(const rendering::PaintImageHandle &image,
                 math::Rect sourcePixels, math::Rect destination,
                 rendering::ImagePaint appearance) override;

  void beginLayer(math::Rect localBounds, float opacity) override;

  void endLayer() override;

  void cancelLayer() noexcept override;

  std::shared_ptr<const rendering::PaintImage>
  capture(math::Rect bounds, math::Vec2f scale,
          const std::function<void(rendering::PaintContext &)> &draw) override;
};

} // namespace playground::sdl
