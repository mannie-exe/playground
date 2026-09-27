#include <cmath>
#include <limits>

#include <ui/content/ContentTypes.hpp>
#include <ui/content/SceneView.hpp>

namespace playground::ui {
namespace {
void validate(const SceneViewProps &props) {
  if (!props.scene || !math::isFinite(props.preferredSize) ||
      !math::hasArea(props.preferredSize) ||
      !std::isfinite(props.resolutionScale) || props.resolutionScale <= 0 ||
      props.resolutionScale > 4)
    throw std::invalid_argument("Invalid scene view properties");
  props.camera.view(props.preferredSize.width / props.preferredSize.height);
  scene::validate({props.camera.view(1),
                   {1, 1},
                   props.clearColor,
                   props.lighting,
                   props.exposure,
                   props.toneMap},
                  {});
  if ((props.aspectRatio &&
       (!std::isfinite(*props.aspectRatio) || *props.aspectRatio <= 0)) ||
      (props.transparentOrder != scene::TransparentOrder::BackToFront &&
       props.transparentOrder != scene::TransparentOrder::Submission))
    throw std::invalid_argument("Invalid scene viewport policy");
}
} // namespace

SceneView::SceneView(SceneViewProps props, layout::BoxProps box)
    : Node{box}, _props{std::move(props)} {
  validate(_props);
}

layout::MeasureResult
SceneView::measureContent(MeasureContext &, const layout::SizeConstraints &) {
  return {.size = _props.preferredSize};
}

void SceneView::prepareContent(PrepareContext &context) {
  _prepared = false;
  if (!context.scenes)
    throw std::logic_error(
        "SceneView requires a frame with scene3D capability");
  const auto viewport = scene::resolveViewport(
      _props.camera,
      {content_detail::contentBounds(bounds(), contentInsets()),
       context.pixelScale, _props.resolutionScale, _props.aspectRatio});
  if (!viewport) {
    _image.reset();
    _viewport.reset();
    _prepared = true;
    return;
  }
  const auto size = viewport->contentBounds.size;
  const auto pixels = viewport->pixelSize;
  const auto imageDomain = context.images ? context.images->resourceDomain()
                                          : rendering::ResourceDomainId::cpu();
  const scene::SceneRenderProps view{viewport->camera,  pixels,
                                     _props.clearColor, _props.lighting,
                                     _props.exposure,   _props.toneMap};
  if (_image && _renderedRevision == _props.scene->revision() &&
      _rendererDomain == context.scenes->resourceDomain() &&
      _imageDomain == imageDomain &&
      _image->pixelSize() == math::Size2{float(pixels.x), float(pixels.y)} &&
      _renderedAspect == size.width / size.height) {
    _viewport = viewport;
    _prepared = true;
    return;
  }
  const auto draws = scene::orderedDraws(view.camera, _props.scene->snapshot(),
                                         _props.transparentOrder);
  auto image = context.scenes->render(view, draws);
  _image = rendering::prepareImage(std::move(image), context.images);
  _viewport = viewport;
  _renderedRevision = _props.scene->revision();
  _rendererDomain = context.scenes->resourceDomain();
  _imageDomain = imageDomain;
  _renderedAspect = size.width / size.height;
  _prepared = true;
  // Scene data is mutable independently of this node; invalidate cached layers.
  invalidatePaint();
}

void SceneView::paint(PaintContext &context) const {
  if (!_prepared)
    throw std::logic_error("SceneView must be prepared before painting");
  if (_image && _viewport)
    context.drawImage(_image, {{}, _image->pixelSize()},
                      _viewport->contentBounds, {});
}

void SceneView::setProps(SceneViewProps props) {
  validate(props);
  _props = std::move(props);
  _image.reset();
  _viewport.reset();
  _prepared = false;
  invalidateLayout();
}

void SceneView::applyPatch(const SceneViewPatch &patch) {
  const SceneViewProps defaults;
  setProps(
      {patch.scene.appliedTo(_props.scene),
       patch.camera.appliedTo(_props.camera, defaults.camera),
       patch.preferredSize.appliedTo(_props.preferredSize,
                                     defaults.preferredSize),
       patch.clearColor.appliedTo(_props.clearColor, defaults.clearColor),
       patch.resolutionScale.appliedTo(_props.resolutionScale,
                                       defaults.resolutionScale),
       patch.aspectRatio.appliedTo(_props.aspectRatio, defaults.aspectRatio),
       patch.transparentOrder.appliedTo(_props.transparentOrder,
                                        defaults.transparentOrder),
       patch.lighting.appliedTo(_props.lighting, defaults.lighting),
       patch.exposure.appliedTo(_props.exposure, defaults.exposure),
       patch.toneMap.appliedTo(_props.toneMap, defaults.toneMap)});
}

} // namespace playground::ui
