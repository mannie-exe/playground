#include <algorithm>

#include <ui/controls/ControlPaint.hpp>
#include <ui/controls/Editing.hpp>
#include <ui/controls/Meter.hpp>

namespace playground::ui {
Meter::Meter(MeterProps p, layout::BoxProps box) : Node{box} {
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
  invalidate(DirtyFlags::Paint | DirtyFlags::Semantics);
}

layout::MeasureResult Meter::measureContent(MeasureContext &,
                                            const layout::SizeConstraints &) {
  return {{160, 16}};
}

void Meter::paint(PaintContext &p) const {
  p.fill({{}, bounds().size}, theme().border);
  if (!_props.value) {
    control_paint::outline(p, {{}, bounds().size}, theme().mutedText, 2);
    return;
  }
  const auto v = *_props.value;
  auto color = _props.critical && v >= *_props.critical
                   ? (theme().highContrast ? theme().text : theme().error)
               : _props.warning && v >= *_props.warning
                   ? (theme().highContrast ? theme().text : theme().warning)
                   : theme().accent;
  p.fill(math::rect(0, 0,
                    bounds().w() *
                        float(std::clamp((v - _props.minimum) /
                                             (_props.maximum - _props.minimum),
                                         0., 1.)),
                    bounds().h()),
         color);
  if (v > _props.maximum)
    control_paint::outline(p, {{}, bounds().size}, theme().text, 2);
  if ((_props.critical && v >= *_props.critical) ||
      (_props.warning && v >= *_props.warning)) {
    const float side = std::min({12.f, bounds().w(), bounds().h()});
    const float x = bounds().w() - side, y = (bounds().h() - side) / 2;
    math::Path2D marker;
    if (_props.critical && v >= *_props.critical) {
      marker.moveTo({x, y}).lineTo({x + side, y + side});
      marker.moveTo({x + side, y}).lineTo({x, y + side});
    } else {
      marker.moveTo({x + side / 2, y})
          .lineTo({x + side, y + side})
          .lineTo({x, y + side})
          .lineTo({x + side / 2, y});
    }
    p.drawPath(marker, {.fill = {}, .stroke = theme().text, .strokeWidth = 2});
  }
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
  if (_glyph == ControlGlyph::Minus || _glyph == ControlGlyph::Plus) {
    p.fill(math::rect(x - 6, y - 1, 12, 2), theme().text);
    if (_glyph == ControlGlyph::Plus)
      p.fill(math::rect(x - 1, y - 6, 2, 12), theme().text);
  } else {
    math::Path2D path;
    float d = _glyph == ControlGlyph::Previous ? -1 : 1;
    path.moveTo({x - d * 3, y - 6})
        .lineTo({x + d * 3, y})
        .lineTo({x - d * 3, y + 6});
    p.drawPath(path, {.fill = {}, .stroke = theme().text, .strokeWidth = 2});
  }
}
} // namespace playground::ui
