#include <stdexcept>

#include <rendering/ResourceLedger.hpp>
#include <ui/containers/Boundaries.hpp>

namespace playground::ui {

Transform::Transform(std::unique_ptr<Node> child, VisualProps visual,
                     layout::BoxProps box)
    : Box{box} {
  setVisualProps(visual);
  if (child)
    setChild(std::move(child));
}

void Clip::setClipProps(ClipProps props) {
  props.cornerRadii.validate();
  if (props == _clipProps)
    return;
  _clipProps = props;
  invalidate(DirtyFlags::Paint | DirtyFlags::HitTest);
}

void Clip::setClipShape(math::ClipShape shape) {
  std::visit(
      [&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, math::Rect>) {
          setClipRect(value);
          setCornerRadii({});
        } else {
          value.validate();
          setClipRect(value.bounds);
          setCornerRadii(value.radii);
        }
      },
      shape);
}

void Clip::applyContentClip(PaintContext &context) const {
  if (cornerRadii() == math::CornerRadii{})
    context.clip(clipBounds());
  else
    context.clipRounded({clipBounds(), cornerRadii()});
}

Clip::Clip(std::unique_ptr<Node> child, std::optional<math::Rect> clip,
           layout::BoxProps box)
    : Box{box} {
  setVisualProps({.overflow = layout::OverflowPolicy::Clip, .clipRect = clip});
  if (child)
    setChild(std::move(child));
}

void Clip::setClipRect(std::optional<math::Rect> rectangle) {
  auto p = visualProps();
  p.clipRect = rectangle;
  setVisualProps(p);
}

void Layer::paintSubtree(PaintContext &context) const {
  const auto bounds = paintBounds();
  if (!bounds.hasArea())
    return;
  if (_props.cachePolicy == LayerCachePolicy::None) {
    Box::paintSubtree(context);
    return;
  }
  const auto scale =
      context.pixelScale() * (_props.rasterScale * _graphicsScale);
  const double bytes = std::ceil(static_cast<double>(bounds.w()) * scale.x) *
                       std::ceil(static_cast<double>(bounds.h()) * scale.y) *
                       context.captureBytesPerPixel();
  if (!std::isfinite(bytes) || bytes > _props.byteLimit) {
    dropCache();
    Box::paintSubtree(context);
    return;
  }
  if (!_cache || _cachedDomain != context.resourceDomain() ||
      _cachedRevision != subtreePaintRevision() || _cachedBounds != bounds ||
      _cachedScale != scale) {
    dropCache();
    Connection reservation;
    if (auto budget = _budget.lock()) {
      const auto count = static_cast<std::size_t>(bytes);
      if (!budget->reserve(count)) {
        Box::paintSubtree(context);
        return;
      }
      reservation =
          Connection{[budget, count]() noexcept { budget->used -= count; }};
    }
    rendering::PaintImageHandle cache;
    bool recordingStarted{};
    const auto uncached = [&] {
      if (recordingStarted)
        throw; // Never replay partially recorded content.
      reservation.disconnect();
      Box::paintSubtree(context);
    };
    try {
      cache = context.capture(bounds, scale, [&](PaintContext &p) {
        recordingStarted = true;
        Box::paintSubtree(p);
      });
    } catch (const rendering::ResourcePressure &) {
      uncached();
      return;
    } catch (const rendering::ResourceAllocationFailure &) {
      uncached();
      return;
    }
    if (!cache || !math::isFinite(cache->pixelSize()) ||
        !math::hasArea(cache->pixelSize()))
      throw std::runtime_error("Paint backend returned an invalid layer image");
    _cache = std::move(cache);
    _cachedDomain = context.resourceDomain();
    _cachedRevision = subtreePaintRevision();
    _cachedBounds = bounds;
    _cachedScale = scale;
    _reservation = std::move(reservation);
  }
  context.drawImage(_cache, {{}, _cache->pixelSize()}, bounds, {});
}

Layer::Layer(std::unique_ptr<Node> child, LayerProps props,
             layout::BoxProps box)
    : Box{box}, _props{props} {
  _props.validate();
  if (child)
    setChild(std::move(child));
}

void Layer::setProps(LayerProps value) {
  value.validate();
  if (value != _props) {
    _props = value;
    dropCache();
    invalidatePaint();
  }
}

void Layer::applyPatch(const LayerPatch &p) {
  const LayerProps d;
  setProps({p.cachePolicy.appliedTo(_props.cachePolicy, d.cachePolicy),
            p.rasterScale.appliedTo(_props.rasterScale, d.rasterScale),
            p.byteLimit.appliedTo(_props.byteLimit, d.byteLimit),
            p.useGraphicsScale.appliedTo(_props.useGraphicsScale,
                                         d.useGraphicsScale)});
}

std::size_t Layer::estimatedCacheBytes() const noexcept {
  return _cache ? static_cast<std::size_t>(_cache->pixelSize().width) *
                      static_cast<std::size_t>(_cache->pixelSize().height) *
                      _cache->bytesPerPixel()
                : 0;
}

} // namespace playground::ui
