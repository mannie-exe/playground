#include <ui/content/Path.hpp>

namespace playground::ui {
void Path::validate(const PathProps &props) {
  if (!math::isFinite(props.viewBox) || !props.viewBox.hasArea())
    throw std::invalid_argument(
        "Path viewBox must have finite positive dimensions");
  props.paint.validate();
  content_detail::validate({.fit = props.fit, .alignment = props.alignment});
  math::flattenPath(props.path);
}

Path::Path(PathProps props, layout::BoxProps box)
    : Node{box}, _props{std::move(props)} {
  validate(_props);
  setHitTestPolicy(HitTestPolicy::None);
}

void Path::setProps(PathProps props) {
  validate(props);
  if (_props == props)
    return;
  const bool layout = _props.viewBox.size != props.viewBox.size;
  _props = std::move(props);
  if (layout)
    invalidateLayout();
  else
    invalidatePaint();
}

void Path::applyPatch(const PathPatch &patch) {
  const PathProps defaults{};
  setProps({patch.path.appliedTo(_props.path),
            patch.viewBox.appliedTo(_props.viewBox),
            patch.paint.appliedTo(_props.paint, defaults.paint),
            patch.fit.appliedTo(_props.fit, defaults.fit),
            patch.alignment.appliedTo(_props.alignment, defaults.alignment)});
}

void Path::paint(PaintContext &context) const {
  const auto area = content_detail::contentBounds(bounds(), contentInsets());
  const auto resolved = content_detail::resolve(
      _props.viewBox, _props.viewBox.size, area,
      {.fit = _props.fit, .alignment = _props.alignment}, _direction);
  if (!resolved.source.hasArea() || !resolved.destination.hasArea())
    return;
  PaintScope scope{context};
  context.clip(area);
  context.translate(math::toVector(resolved.destination.position));
  context.transform(math::Transform2D::scaling(
      {resolved.destination.w() / resolved.source.w(),
       resolved.destination.h() / resolved.source.h()}));
  context.translate(-math::toVector(resolved.source.position));
  context.drawPath(_props.path, _props.paint);
}
} // namespace playground::ui
