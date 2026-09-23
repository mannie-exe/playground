#include <platform/sdl/SurfacePainter.hpp>

#include <algorithm>
#include <cmath>
#include <platform/sdl/SDLGeometry.hpp>

namespace playground::sdl {

struct SurfacePainter::ModulationGuard {
  SDL_Surface *surface;
  Uint8 r{}, g{}, b{}, alpha{};
  SDL_BlendMode blend{};

  explicit ModulationGuard(SDL_Surface *value) : surface{value} {
    if (!SDL_GetSurfaceColorMod(surface, &r, &g, &b) ||
        !SDL_GetSurfaceAlphaMod(surface, &alpha) ||
        !SDL_GetSurfaceBlendMode(surface, &blend))
      throw std::runtime_error(SDL_GetError());
  }
  ~ModulationGuard() {
    SDL_SetSurfaceColorMod(surface, r, g, b);
    SDL_SetSurfaceAlphaMod(surface, alpha);
    SDL_SetSurfaceBlendMode(surface, blend);
  }
};

template <typename Sample>
void SurfacePainter::raster(math::Rect local, Sample sample) {
  validate(local);
  if (!_target || !local.hasArea())
    return;
  const auto inverse = _state.transform.inverse();
  if (!inverse)
    return;
  const auto extent =
      math::intersect(math::intersect(mapped(local), _state.clip),
                      math::rect(float(_origin.x), float(_origin.y),
                                 float(_target->w), float(_target->h)));
  if (!extent.hasArea())
    return;
  const auto pixels = toPixelRect(extent, PixelRounding::Outward);
  constexpr int samples = 4;
  for (int y = pixels.y; y < pixels.y + pixels.h; ++y)
    for (int x = pixels.x; x < pixels.x + pixels.w; ++x) {
      Pixel source;
      for (int sy = 0; sy < samples; ++sy)
        for (int sx = 0; sx < samples; ++sx) {
          const math::Point2 global{float(x) + (sx + 0.5f) / samples,
                                    float(y) + (sy + 0.5f) / samples};
          if (!_state.clip.contains(global))
            continue;
          bool inside = true;
          for (const auto &mask : _state.masks)
            if (!mask.shape.contains(mask.inverse.mapPoint(global))) {
              inside = false;
              break;
            }
          const auto point = inverse->mapPoint(global);
          if (inside && local.contains(point))
            source += sample(point) * (1.0 / (samples * samples));
        }
      if (source.a == 0)
        continue;
      const int tx = x - _origin.x, ty = y - _origin.y;
      const auto destination = read(*_target, tx, ty, true);
      auto byte = [](double v) {
        return static_cast<Uint8>(std::lround(std::clamp(v, 0.0, 1.0) * 255));
      };
      require(SDL_WriteSurfacePixel(
          _target, tx, ty, byte(source.r + destination.r * (1 - source.a)),
          byte(source.g + destination.g * (1 - source.a)),
          byte(source.b + destination.b * (1 - source.a)),
          byte(source.a + destination.a * (1 - source.a))));
    }
}

void SurfacePainter::require(bool result) {
  if (!result)
    throw std::runtime_error(std::string{"SDL surface paint: "} +
                             SDL_GetError());
}

void SurfacePainter::validate(math::Rect bounds) {
  if (!math::isFinite(bounds) || !math::isNonNegative(bounds.size))
    throw std::invalid_argument(
        "Paint rectangle must be finite and nonnegative");
}

SDL_Rect SurfacePainter::targetRect(math::Rect global) const {
  const auto pixels = toPixelRect(global, PixelRounding::Nearest);
  return {checkedPixel(static_cast<double>(pixels.x) - _origin.x),
          checkedPixel(static_cast<double>(pixels.y) - _origin.y), pixels.w,
          pixels.h};
}

math::Rect SurfacePainter::mapped(math::Rect local) const {
  validate(local);
  auto result = _state.transform.mapBounds(local);
  validate(result);
  return result;
}

bool SurfacePainter::applyClip() {
  if (!_target || !_state.clip.hasArea())
    return false;
  const auto rectangle = targetRect(_state.clip);
  // SDL returns false for an empty intersection, not an SDL failure.
  return SDL_SetSurfaceClipRect(_target, &rectangle);
}

void SurfacePainter::restoreLayer() noexcept {
  auto &layer = _layers.back();
  if (layer.restored)
    return;
  _target = layer.parent;
  _origin = layer.parentOrigin;
  _state = std::move(layer.state);
  _layerBytes -= layer.bytes;
  if (_states.size() > layer.saveDepth)
    _states.resize(layer.saveDepth);
  layer.restored = true;
}

SurfacePainter::Pixel SurfacePainter::read(SDL_Surface &surface, int x, int y,
                                           bool premultiplied) {
  Uint8 r{}, g{}, b{}, a{};
  require(SDL_ReadSurfacePixel(&surface, x, y, &r, &g, &b, &a));
  const double alpha = a / 255.0;
  const double factor = premultiplied ? 1.0 : alpha;
  return {r / 255.0 * factor, g / 255.0 * factor, b / 255.0 * factor, alpha};
}

SurfacePainter::Pixel SurfacePainter::solid(math::ColorRGBA8 color) {
  const double alpha = color.a / 255.0;
  return {color.r / 255.0 * alpha, color.g / 255.0 * alpha,
          color.b / 255.0 * alpha, alpha};
}

void SurfacePainter::rasterImage(const SurfacePaintImage &image,
                                 math::Rect source, math::Rect destination,
                                 ui::ImagePaint appearance) {
  auto &surface = *image.surface();
  const int left = std::max(0, int(std::floor(source.left())));
  const int top = std::max(0, int(std::floor(source.top())));
  const int right = std::min(surface.w - 1, int(std::ceil(source.right())) - 1);
  const int bottom =
      std::min(surface.h - 1, int(std::ceil(source.bottom())) - 1);
  raster(destination, [&](math::Point2 point) {
    const double x =
        source.x() +
        (point.x - destination.x()) / destination.w() * source.w() - 0.5;
    const double y =
        source.y() +
        (point.y - destination.y()) / destination.h() * source.h() - 0.5;
    auto fetch = [&](int px, int py) {
      return read(surface, std::clamp(px, left, right),
                  std::clamp(py, top, bottom), image.isPremultiplied());
    };
    Pixel value;
    if (appearance.sampling == ui::Sampling::Nearest)
      value = fetch(int(std::floor(x + 0.5)), int(std::floor(y + 0.5)));
    else {
      const int ix = int(std::floor(x)), iy = int(std::floor(y));
      const double dx = x - ix, dy = y - iy;
      value += fetch(ix, iy) * ((1 - dx) * (1 - dy));
      value += fetch(ix + 1, iy) * (dx * (1 - dy));
      value += fetch(ix, iy + 1) * ((1 - dx) * dy);
      value += fetch(ix + 1, iy + 1) * (dx * dy);
    }
    const double opacity = appearance.tint.a / 255.0;
    return Pixel{value.r * appearance.tint.r / 255.0 * opacity,
                 value.g * appearance.tint.g / 255.0 * opacity,
                 value.b * appearance.tint.b / 255.0 * opacity,
                 value.a * opacity};
  });
}

SurfacePainter::SurfacePainter(SDL_Surface &target, SurfacePainterProps props)
    : _props{props}, _root{target}, _target{&target} {
  if (!math::isFinite(props.pixelScale) || !math::isPositive(props.pixelScale))
    throw std::invalid_argument(
        "Painter pixel scale must be finite and positive");
  require(SDL_GetSurfaceClipRect(&target, &_originalClip));
  _state = {math::Transform2D::scaling(props.pixelScale),
            fromSDL(_originalClip)};
}

SurfacePainter::~SurfacePainter() {
  while (!_layers.empty())
    cancelLayer();
  SDL_SetSurfaceClipRect(&_root, &_originalClip);
}

void SurfacePainter::restore() noexcept {
  if (!_states.empty() &&
      (_layers.empty() || _states.size() > _layers.back().saveDepth)) {
    _state = std::move(_states.back());
    _states.pop_back();
  }
}

void SurfacePainter::transform(math::Transform2D value) {
  auto finite = [](math::Transform2D t) {
    return std::isfinite(t.a) && std::isfinite(t.b) && std::isfinite(t.c) &&
           std::isfinite(t.d) && std::isfinite(t.tx) && std::isfinite(t.ty);
  };
  if (!finite(value))
    throw std::invalid_argument("Paint transform must be finite");
  const auto next = _state.transform * value;
  if (!finite(next))
    throw std::overflow_error("Paint transform exceeds float range");
  _state.transform = next;
}

void SurfacePainter::clip(math::Rect rectangle) {
  validate(rectangle);
  if (_state.transform.b != 0 || _state.transform.c != 0)
    clipRounded({rectangle, {}});
  else
    _state.clip = math::intersect(_state.clip, mapped(rectangle));
}

void SurfacePainter::clipRounded(math::RoundedRect shape) {
  shape.validate();
  const auto inverse = _state.transform.inverse();
  if (!inverse) {
    _state.clip = {};
    return;
  }
  _state.masks.push_back({shape, *inverse});
  _state.clip = math::intersect(_state.clip, mapped(shape.bounds));
}

void SurfacePainter::fillRounded(math::RoundedRect shape,
                                 math::ColorRGBA8 color) {
  shape.validate();
  raster(shape.bounds, [&](math::Point2 p) {
    return shape.contains(p) ? solid(color) : Pixel{};
  });
}

void SurfacePainter::strokeRounded(math::RoundedRect shape, math::Insets widths,
                                   math::ColorRGBA8 color) {
  shape.validate();
  if (!math::isFinite(widths) || !math::isNonNegative(widths))
    throw std::invalid_argument("Border widths must be finite and nonnegative");
  const auto inner = shape.inset(widths);
  raster(shape.bounds, [&](math::Point2 p) {
    return shape.contains(p) && !inner.contains(p) ? solid(color) : Pixel{};
  });
}

void SurfacePainter::paintRoundedBox(math::RoundedRect shape,
                                     math::Insets widths, math::ColorRGBA8 fill,
                                     std::optional<math::ColorRGBA8> border) {
  const auto inner = shape.inset(widths);
  // Resolve border and fill at each sample, then composite once. Two separate
  // coverage blends leave a translucent seam where opaque regions meet.
  raster(shape.bounds, [&](math::Point2 point) {
    if (!shape.contains(point))
      return Pixel{};
    if (inner.contains(point))
      return solid(fill);
    return border ? solid(*border) : Pixel{};
  });
}

void SurfacePainter::fill(math::Rect rectangle, math::ColorRGBA8 color) {
  if (!simple()) {
    raster(rectangle, [&](math::Point2) { return solid(color); });
    return;
  }
  const auto destination = targetRect(mapped(rectangle));
  if (!destination.w || !destination.h || !color.a || !applyClip())
    return;
  if (color.a == 255) {
    require(SDL_FillSurfaceRect(
        _target, &destination,
        SDL_MapSurfaceRGBA(_target, color.r, color.g, color.b, color.a)));
    return;
  }
  if (!_colorPixel) {
    _colorPixel.reset(SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32));
    if (!_colorPixel)
      throw std::runtime_error(SDL_GetError());
    require(SDL_SetSurfaceBlendMode(_colorPixel.get(), SDL_BLENDMODE_BLEND));
  }
  require(SDL_FillSurfaceRect(_colorPixel.get(), nullptr,
                              SDL_MapSurfaceRGBA(_colorPixel.get(), color.r,
                                                 color.g, color.b, color.a)));
  require(SDL_BlitSurfaceScaled(_colorPixel.get(), nullptr, _target,
                                &destination, SDL_SCALEMODE_NEAREST));
}

void SurfacePainter::drawImage(const ui::PaintImageHandle &image,
                               math::Rect sourcePixels, math::Rect destination,
                               ui::ImagePaint appearance) {
  const auto *surface = dynamic_cast<const SurfacePaintImage *>(image.get());
  if (!surface)
    throw std::invalid_argument("Paint image belongs to another backend");
  validate(sourcePixels);
  const auto size = image->pixelSize();
  if (sourcePixels.left() < 0 || sourcePixels.top() < 0 ||
      sourcePixels.right() > size.width || sourcePixels.bottom() > size.height)
    throw std::invalid_argument("Source rectangle exceeds paint image");
  validate(destination);
  if (surface->surface().get() == _target)
    throw std::invalid_argument("Paint source cannot be the active target");
  if (appearance.sampling != ui::Sampling::Nearest &&
      appearance.sampling != ui::Sampling::Linear)
    throw std::invalid_argument("Unknown image sampling mode");
  if (!sourcePixels.hasArea() || !destination.hasArea())
    return;
  if (!simple()) {
    rasterImage(*surface, sourcePixels, destination, appearance);
    return;
  }
  auto src = toPixelRect(sourcePixels, PixelRounding::Nearest);
  auto dst = targetRect(mapped(destination));
  if (!src.w || !src.h || !dst.w || !dst.h || !appearance.tint.a ||
      !applyClip())
    return;
  if (appearance.sampling != ui::Sampling::Nearest &&
      appearance.sampling != ui::Sampling::Linear)
    throw std::invalid_argument("Unknown image sampling mode");
  auto *raw = surface->surface().get();
  if (raw == _target)
    throw std::invalid_argument("Paint source cannot be the active target");
  ModulationGuard guard{raw};
  const auto mod = [&](Uint8 value) {
    return surface->isPremultiplied()
               ? static_cast<Uint8>(
                     (static_cast<unsigned>(value) * appearance.tint.a + 127) /
                     255)
               : value;
  };
  require(SDL_SetSurfaceColorMod(raw, mod(appearance.tint.r),
                                 mod(appearance.tint.g),
                                 mod(appearance.tint.b)));
  require(SDL_SetSurfaceAlphaMod(raw, appearance.tint.a));
  require(SDL_SetSurfaceBlendMode(raw, surface->isPremultiplied()
                                           ? SDL_BLENDMODE_BLEND_PREMULTIPLIED
                                           : SDL_BLENDMODE_BLEND));
  require(SDL_BlitSurfaceScaled(raw, &src, _target, &dst,
                                appearance.sampling == ui::Sampling::Nearest
                                    ? SDL_SCALEMODE_NEAREST
                                    : SDL_SCALEMODE_LINEAR));
}

void SurfacePainter::beginLayer(math::Rect localBounds, float opacity) {
  if (!std::isfinite(opacity) || opacity < 0 || opacity > 1)
    throw std::invalid_argument("Layer opacity must be between zero and one");
  auto bounds = math::intersect(mapped(localBounds), _state.clip);
  const auto pixels = toPixelRect(bounds, PixelRounding::Outward);
  const auto bytes = static_cast<std::uint64_t>(pixels.w) * pixels.h * 4;
  if (bytes > _props.layerByteBudget ||
      bytes > _props.layerByteBudget - _layerBytes)
    throw std::length_error("UI layer exceeds configured byte budget");
  OwnedSurface surface;
  if (pixels.w && pixels.h && _target && opacity > 0) {
    surface.reset(
        SDL_CreateSurface(pixels.w, pixels.h, SDL_PIXELFORMAT_RGBA32));
    if (!surface)
      throw std::runtime_error(SDL_GetError());
    require(SDL_FillSurfaceRect(surface.get(), nullptr, 0));
  }
  _layers.push_back({std::move(surface),
                     _target,
                     _origin,
                     {pixels.x, pixels.y},
                     _state,
                     opacity,
                     static_cast<std::size_t>(bytes),
                     _states.size()});
  _target = _layers.back().surface.get();
  _origin = {pixels.x, pixels.y};
  _state.clip = bounds;
  _state.masks.clear();
  _layerBytes += static_cast<std::size_t>(bytes);
}

void SurfacePainter::endLayer() {
  if (_layers.empty())
    throw std::logic_error("No paint layer to end");
  if (_states.size() != _layers.back().saveDepth)
    throw std::logic_error("Unbalanced save/restore within paint layer");
  auto &layer = _layers.back();
  restoreLayer();
  try {
    if (layer.surface && !_state.masks.empty()) {
      // Apply the inherited shape mask once to the composited group.
      const auto oldTransform = _state.transform;
      _state.transform = {};
      try {
        SurfaceHandle borrowed{layer.surface.get(), [](SDL_Surface *) {}};
        rasterImage(
            SurfacePaintImage{std::move(borrowed), true},
            math::rect(0, 0, float(layer.surface->w), float(layer.surface->h)),
            math::rect(float(layer.origin.x), float(layer.origin.y),
                       float(layer.surface->w), float(layer.surface->h)),
            {.tint = {255, 255, 255,
                      static_cast<Uint8>(std::lround(layer.opacity * 255))},
             .sampling = ui::Sampling::Nearest});
      } catch (...) {
        _state.transform = oldTransform;
        throw;
      }
      _state.transform = oldTransform;
    } else if (layer.surface && applyClip()) {
      const auto modulation =
          static_cast<Uint8>(std::lround(layer.opacity * 255));
      // Drawing onto transparent black yields premultiplied RGB. Modulate
      // both RGB and alpha, then composite once; per-child opacity is not
      // equivalent.
      require(SDL_SetSurfaceColorMod(layer.surface.get(), modulation,
                                     modulation, modulation));
      require(SDL_SetSurfaceAlphaMod(layer.surface.get(), modulation));
      require(SDL_SetSurfaceBlendMode(layer.surface.get(),
                                      SDL_BLENDMODE_BLEND_PREMULTIPLIED));
      SDL_Rect destination{
          checkedPixel(static_cast<double>(layer.origin.x) - _origin.x),
          checkedPixel(static_cast<double>(layer.origin.y) - _origin.y),
          layer.surface->w, layer.surface->h};
      require(
          SDL_BlitSurface(layer.surface.get(), nullptr, _target, &destination));
    }
  } catch (...) {
    throw;
  }
  _layers.pop_back();
}

void SurfacePainter::cancelLayer() noexcept {
  if (!_layers.empty()) {
    restoreLayer();
    _layers.pop_back();
  }
}

std::shared_ptr<const ui::PaintImage>
SurfacePainter::capture(math::Rect bounds, math::Vec2f scale,
                        const std::function<void(ui::PaintContext &)> &draw) {
  validate(bounds);
  if (!math::isFinite(scale) || !math::isPositive(scale))
    throw std::invalid_argument("Capture scale must be finite and positive");
  const int width =
      std::max(1, checkedPixel(static_cast<double>(bounds.w()) * scale.x,
                               PixelRounding::Ceil));
  const int height =
      std::max(1, checkedPixel(static_cast<double>(bounds.h()) * scale.y,
                               PixelRounding::Ceil));
  if (static_cast<std::uint64_t>(width) * height * 4 > _props.layerByteBudget)
    throw std::length_error("Cached layer exceeds backend byte budget");
  SurfaceHandle surface{
      SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32),
      SurfaceHandleDeleter{}};
  if (!surface)
    throw std::runtime_error(SDL_GetError());
  require(SDL_FillSurfaceRect(surface.get(), nullptr, 0));
  {
    SurfacePainter painter{*surface,
                           SurfacePainterProps{scale, _props.layerByteBudget}};
    painter.translate({-bounds.x(), -bounds.y()});
    draw(painter);
  }
  return std::make_shared<SurfacePaintImage>(std::move(surface), true);
}

} // namespace playground::sdl
