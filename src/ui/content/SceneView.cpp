#include <cmath>
#include <limits>

#include <ui/content/ContentTypes.hpp>
#include <ui/content/SceneView.hpp>

namespace playground::ui {
namespace {
scene::CameraProps camera(const SceneViewProps &props) {
  return props.worldScene
             ? props.worldScene->camera().localCamera(
                   props.worldScene->origin(), props.worldScene->limits())
             : props.camera;
}

void validate(const SceneViewProps &props) {
  if (bool(props.scene) == bool(props.worldScene) ||
      !math::isFinite(props.preferredSize) ||
      !math::hasArea(props.preferredSize) ||
      !std::isfinite(props.resolutionScale) || props.resolutionScale <= 0 ||
      props.resolutionScale > 4)
    throw std::invalid_argument("Invalid scene view properties");
  camera(props).view(props.preferredSize.width / props.preferredSize.height);
  scene::validate({camera(props).view(1),
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
  const float scale =
      context.graphics
          ? (_props.adaptiveResolution
                 ? context.graphics->sceneScale
                 : context.graphics->requested.threeD.resolutionScale)
          : 1;
  const auto sampling = context.graphics
                            ? context.graphics->requested.threeD.reconstruction
                            : rendering::Sampling::Linear;
  if (sampling != _sampling) {
    _sampling = sampling;
    invalidatePaint();
  }
  auto viewport = scene::resolveViewport(
      camera(_props),
      {content_detail::contentBounds(bounds(), contentInsets()),
       context.pixelScale, _props.resolutionScale * scale, _props.aspectRatio});
  if (!viewport) {
    _image.reset();
    _viewport.reset();
    _prepared = true;
    return;
  }
  const auto size = viewport->contentBounds.size;
  if (_props.worldScene) {
    viewport->origin = _props.worldScene->origin();
    viewport->spatialLimits = _props.worldScene->limits();
    viewport->camera = _props.worldScene->camera().view(
        *viewport->origin, viewport->spatialLimits, size.width / size.height);
  }
  const auto pixels = viewport->pixelSize;
  const auto imageDomain = context.images ? context.images->resourceDomain()
                                          : rendering::ResourceDomainId::cpu();
  if (_rendererDomain != context.scenes->resourceDomain() ||
      _imageDomain != imageDomain)
    _image.reset(); // Old-domain output cannot be reused during replacement.
  scene::SceneRenderProps view{viewport->camera,  pixels,
                               _props.clearColor, _props.lighting,
                               _props.exposure,   _props.toneMap};
  view.resourceOwner =
      _props.worldScene ? _props.worldScene->resourceOwner() : _resourceOwner;
  view.workloadId = _props.adaptiveResolution ? _workload : 0;
  view.qualityRevision = context.graphics ? context.graphics->revision : 0;
  const auto revision = _props.worldScene
                            ? _props.worldScene->world().revision()
                            : _props.scene->revision();
  if (_image && _renderedRevision == revision &&
      _rendererDomain == context.scenes->resourceDomain() &&
      _imageDomain == imageDomain &&
      _image->pixelSize() == math::Size2{float(pixels.x), float(pixels.y)} &&
      _renderedAspect == size.width / size.height) {
    _viewport = viewport;
    _prepared = true;
    return;
  }
  const auto snapshot =
      _props.scene ? _props.scene->snapshot() : std::vector<scene::MeshDraw>{};
  const auto draws = scene::orderedDraws(
      view.camera,
      _props.worldScene ? _props.worldScene->draws()
                        : std::span<const scene::MeshDraw>{snapshot},
      _props.transparentOrder);
  auto image = context.scenes->render(view, draws);
  _image = rendering::prepareImage(std::move(image), context.images);
  _viewport = viewport;
  _renderedRevision = revision;
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
                      _viewport->contentBounds, {.sampling = _sampling});
}

void SceneView::setProps(SceneViewProps props) {
  if (props == _props)
    return;
  validate(props);
  const bool layoutChanged = props.preferredSize != _props.preferredSize;
  if (props.scene != _props.scene)
    _resourceOwner = std::make_shared<const int>(0);
  _props = std::move(props);
  _image.reset();
  _viewport.reset();
  _prepared = false;
  if (layoutChanged)
    invalidateLayout();
  else
    invalidatePaint();
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
       patch.toneMap.appliedTo(_props.toneMap, defaults.toneMap),
       patch.adaptiveResolution.appliedTo(_props.adaptiveResolution,
                                          defaults.adaptiveResolution),
       patch.worldScene.appliedTo(_props.worldScene)});
}

} // namespace playground::ui
