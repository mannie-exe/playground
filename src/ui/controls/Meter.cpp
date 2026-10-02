#include <algorithm>

#include <ui/controls/ControlPaint.hpp>
#include <ui/controls/Editing.hpp>
#include <ui/controls/Meter.hpp>

namespace playground::ui {
Meter::Meter(MeterProps p, layout::BoxProps box)
    : Meter{{}, {}, std::move(p), box} {}

Meter::Meter(std::unique_ptr<TintableContent> warningIcon,
             std::unique_ptr<TintableContent> criticalIcon, MeterProps p,
             layout::BoxProps box)
    : Node{box} {
  const auto attach =
      [this](std::unique_ptr<TintableContent> icon) -> TintableContent * {
    if (!icon)
      return nullptr;
    icon->setInert(true);
    icon->setSemanticProps({.exposure = SemanticExposure::HiddenSubtree});
    auto *result = icon.get();
    appendChild(std::move(icon));
    return result;
  };
  _warningIcon = attach(std::move(warningIcon));
  _criticalIcon = attach(std::move(criticalIcon));
  setProps(std::move(p));
}

void Meter::setProps(MeterProps p) {
  if (!std::isfinite(p.minimum) || !std::isfinite(p.maximum) ||
      p.maximum <= p.minimum || !std::isfinite(p.maximum - p.minimum) ||
      (p.value && !std::isfinite(*p.value)) ||
      (p.warning && !std::isfinite(*p.warning)) ||
      (p.critical && !std::isfinite(*p.critical)) ||
      (p.warning && p.critical && *p.warning > *p.critical))
    throw std::invalid_argument("Invalid meter range or thresholds");
  _props = std::move(p);
  if (_warningIcon)
    _warningIcon->setVisibility(warning() && !critical() ? Visibility::Visible
                                                         : Visibility::Hidden);
  if (_criticalIcon)
    _criticalIcon->setVisibility(critical() ? Visibility::Visible
                                            : Visibility::Hidden);
  invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
}

layout::MeasureResult Meter::measureContent(MeasureContext &context,
                                            const layout::SizeConstraints &) {
  const auto &m = themeMetrics();
  for (const auto &icon : children())
    icon->measure(context, layout::SizeConstraints::tight(
                               {m.meterMarker, m.meterMarker}));
  return {{m.meterLength, m.meterHeight}};
}

math::Rect Meter::markerBounds() const noexcept {
  const auto track = trackBounds();
  const float side =
      std::min({themeMetrics().meterMarker, track.w(), track.h()});
  const float inset =
      std::min(themeMetrics().meterMarkerInset, track.w() - side);
  return math::rect(track.right() - inset - side,
                    track.y() + (track.h() - side) / 2, side, side);
}

math::Rect Meter::trackBounds() const noexcept {
  const float height = std::min(themeMetrics().meterHeight, bounds().h());
  return math::rect(0, (bounds().h() - height) / 2, bounds().w(), height);
}

void Meter::arrangeChildren(ArrangeContext &context, math::Rect) {
  for (const auto &icon : children())
    icon->arrange(context, markerBounds());
}

void Meter::paintSubtree(PaintContext &p) const {
  const auto track = trackBounds();
  p.fill(track, theme().border);
  if (!_props.value) {
    control_paint::outline(p, track, theme().mutedText,
                           themeMetrics().indicatorStroke);
    return;
  }
  const auto v = *_props.value;
  auto color = critical()  ? theme().error
               : warning() ? theme().warning
                           : theme().accent;
  auto filled = track;
  filled.size.width *= float(std::clamp(
      (v - _props.minimum) / (_props.maximum - _props.minimum), 0., 1.));
  p.fill(filled, color);
  if (v > _props.maximum)
    control_paint::outline(p, track, theme().text,
                           themeMetrics().indicatorStroke);
  if (!critical() && !warning())
    return;
  const auto draw = [&](math::Rect region, math::ColorRGBA8 ink) {
    if (!math::intersect(region, markerBounds()).hasArea())
      return;
    PaintScope scope{p};
    p.clip(region);
    paintMarker(p, ink);
  };
  draw(filled, critical() ? theme().onError : theme().onWarning);
  draw(math::rect(filled.right(), track.y(), track.right() - filled.right(),
                  track.h()),
       theme().meterOnTrack);
}

void Meter::paintMarker(PaintContext &p, math::ColorRGBA8 ink) const {
  if (auto *icon = critical() ? _criticalIcon : _warningIcon) {
    PaintScope scope{p};
    p.transform(icon->localTransform());
    icon->paintTinted(p, ink);
    return;
  }
  const auto slot = markerBounds();
  const float stroke = std::min(themeMetrics().indicatorStroke, slot.w() / 2);
  if (stroke <= 0)
    return;
  const auto r = math::inset(slot, math::Insets::all(stroke / 2));
  math::Path2D marker;
  if (critical()) {
    marker.moveTo({r.x(), r.y()}).lineTo({r.right(), r.bottom()});
    marker.moveTo({r.right(), r.y()}).lineTo({r.x(), r.bottom()});
  } else {
    marker.moveTo({r.x() + r.w() / 2, r.y()})
        .lineTo({r.right(), r.bottom()})
        .lineTo({r.x(), r.bottom()})
        .lineTo({r.x() + r.w() / 2, r.y()});
  }
  p.drawPath(marker, {.fill = {}, .stroke = ink, .strokeWidth = stroke});
}

SemanticState Meter::semanticState() const {
  auto s = Node::semanticState();
  s.description.role = SemanticRole::Meter;
  s.description.name = _props.name;
  s.readOnly = true;
  s.description.value =
      _props.value ? formatNumber(*_props.value) + " " + _props.unit + " / " +
                         formatNumber(_props.maximum) + " " + _props.unit +
                         (*_props.value > _props.maximum ? " (over limit)" : "")
                   : "Unavailable";
  if (_props.value) {
    if (_props.critical && *_props.value >= *_props.critical)
      *s.description.value += " (critical)";
    else if (_props.warning && *_props.value >= *_props.warning)
      *s.description.value += " (warning)";
  }
  if (_props.value)
    s.range =
        RangeValue{std::clamp(*_props.value, _props.minimum, _props.maximum),
                   _props.minimum, _props.maximum, 1};
  return s;
}

void ControlIcon::paint(PaintContext &p) const {
  const float x = bounds().w() / 2, y = bounds().h() / 2;
  const float half = std::min({themeMetrics().iconSize * .375f, x, y});
  const float stroke = std::min(themeMetrics().indicatorStroke, 2 * half);
  bool enabled = true;
  for (auto *p = parent(); p; p = p->parent())
    enabled &= p->isInteractionEnabled();
  const auto ink = enabled ? theme().text : theme().mutedText;
  if (_glyph == ControlGlyph::Minus || _glyph == ControlGlyph::Plus) {
    p.fill(math::rect(x - half, y - stroke / 2, 2 * half, stroke), ink);
    if (_glyph == ControlGlyph::Plus)
      p.fill(math::rect(x - stroke / 2, y - half, stroke, 2 * half), ink);
  } else {
    math::Path2D path;
    float d = _glyph == ControlGlyph::Previous ? -1 : 1;
    path.moveTo({x - d * half / 2, y - half})
        .lineTo({x + d * half / 2, y})
        .lineTo({x - d * half / 2, y + half});
    p.drawPath(path, {.fill = {},
                      .stroke = ink,
                      .strokeWidth = themeMetrics().indicatorStroke});
  }
}
} // namespace playground::ui
