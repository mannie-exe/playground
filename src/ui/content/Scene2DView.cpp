#include <ui/content/ContentTypes.hpp>
#include <ui/content/Scene2DView.hpp>

namespace playground::ui {
namespace {
void validate(const Scene2DViewProps &props) {
  if (!props.scene || !math::isFinite(props.preferredSize) ||
      !math::hasArea(props.preferredSize) || !props.camera.inverse())
    throw std::invalid_argument("Invalid 2D scene view");
}
} // namespace

Scene2DView::Scene2DView(Scene2DViewProps props, layout::BoxProps box)
    : Node{box}, _props{std::move(props)} {
  validate(_props);
}

void Scene2DView::setProps(Scene2DViewProps props) {
  validate(props);
  _props = std::move(props);
  _preparedRevision.reset();
  _prepared = false;
  invalidateLayout();
}

void Scene2DView::applyPatch(const Scene2DViewPatch &patch) {
  const Scene2DViewProps defaults;
  setProps({patch.scene.appliedTo(_props.scene),
            patch.camera.appliedTo(_props.camera, defaults.camera),
            patch.preferredSize.appliedTo(_props.preferredSize,
                                          defaults.preferredSize)});
}

layout::MeasureResult
Scene2DView::measureContent(MeasureContext &, const layout::SizeConstraints &) {
  return {.size = _props.preferredSize};
}

void Scene2DView::prepareContent(PrepareContext &context) {
  _prepared = false;
  const auto domain = context.images ? context.images->resourceDomain()
                                     : rendering::ResourceDomainId::cpu();
  if (_preparedRevision == _props.scene->revision() && _imageDomain == domain) {
    _prepared = true;
    return;
  }
  auto snapshot = _props.scene->snapshot();
  for (auto &item : snapshot)
    if (item.image)
      item.image = rendering::prepareImage(item.image, context.images);
  _snapshot = std::move(snapshot);
  _preparedRevision = _props.scene->revision();
  _imageDomain = domain;
  _prepared = true;
  invalidatePaint();
}

void Scene2DView::paint(PaintContext &context) const {
  if (!_prepared)
    throw std::logic_error("Scene2DView must be prepared before painting");
  PaintScope scope{context};
  const auto area = content_detail::contentBounds(bounds(), contentInsets());
  context.clip(area);
  context.translate(math::toVector(area.position));
  context.transform(_props.camera);
  for (const auto &item : _snapshot) {
    PaintScope itemScope{context};
    context.transform(item.transform);
    if (item.image)
      context.drawImage(item.image, {{}, item.image->pixelSize()}, item.bounds,
                        item.paint);
    else
      context.fill(item.bounds, item.paint.tint);
  }
}
} // namespace playground::ui
