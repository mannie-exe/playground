#include <platform/sdl/SDLGeometry.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <ui/content/Vector.hpp>

namespace playground::ui {

void Vector::validate(const VectorProps &props) {
  if ((std::holds_alternative<std::string>(props.source) &&
       std::get<std::string>(props.source).empty()) ||
      (std::holds_alternative<SVGDocumentHandle>(props.source) &&
       !std::get<SVGDocumentHandle>(props.source)))
    throw std::invalid_argument("Vector requires an SVG path or document");
  if (props.intrinsicSize && (!math::isFinite(*props.intrinsicSize) ||
                              !math::hasArea(*props.intrinsicSize)))
    throw std::invalid_argument(
        "Vector intrinsic dimensions must be finite and positive");
  if (!std::isfinite(props.rasterScale) || props.rasterScale <= 0)
    throw std::invalid_argument(
        "Vector raster scale must be finite and positive");
  if (!props.maximumRasterPixels)
    throw std::invalid_argument("Vector raster pixel budget must be positive");
  content_detail::validate(props.content);
}

void Vector::prepareContent(PrepareContext &context) {
  _prepared = false;
  const auto natural = naturalSize();
  const auto resolved = content_detail::resolve(
      {{}, natural}, natural,
      content_detail::contentBounds(bounds(), contentInsets()), _props.content,
      _direction);
  if (!resolved.destination.hasArea() || !resolved.source.hasArea()) {
    _resolved = resolved;
    _prepared = true;
    return;
  }
  // Rasterize the entire SVG at the scale implied by the visible crop. Cover
  // crops after rasterization; it must not squash the complete SVG into the
  // box.
  const math::Vec2i requested{
      std::max(1, sdl::checkedPixel(
                      static_cast<double>(natural.width) *
                          resolved.destination.w() / resolved.source.w() *
                          context.pixelScale.x * _props.rasterScale,
                      sdl::PixelRounding::Ceil)),
      std::max(1, sdl::checkedPixel(
                      static_cast<double>(natural.height) *
                          resolved.destination.h() / resolved.source.h() *
                          context.pixelScale.y * _props.rasterScale,
                      sdl::PixelRounding::Ceil))};
  if (static_cast<std::uint64_t>(requested.x) * requested.y >
      _props.maximumRasterPixels)
    throw std::length_error("Vector raster exceeds configured pixel budget");
  if (!_source || requested != _rasterSize) {
    auto raster = sdl::makeSurfaceImage(
        _assets.getVector(_props.source, _props.styles, requested));
    _source = std::move(raster);
    _rasterSize = requested;
  }
  _raster = rendering::prepareImage(_source, context.images);
  const auto pixels = _raster->pixelSize();
  _resolved = {{{resolved.source.x() / natural.width * pixels.width,
                 resolved.source.y() / natural.height * pixels.height},
                {resolved.source.w() / natural.width * pixels.width,
                 resolved.source.h() / natural.height * pixels.height}},
               resolved.destination};
  // Clamp float edge roundoff at the resource boundary.
  _resolved.source.size.width =
      std::min(_resolved.source.w(), pixels.width - _resolved.source.x());
  _resolved.source.size.height =
      std::min(_resolved.source.h(), pixels.height - _resolved.source.y());
  _prepared = true;
}

void Vector::paint(PaintContext &context) const {
  if (!_prepared)
    throw std::logic_error(
        "Vector must be prepared after layout and property changes");
  if (_raster && _resolved.destination.hasArea()) {
    auto paint = _props.content.paint;
    if (_props.useTheme) {
      bool enabled = true;
      for (const Node *node = this; node; node = node->parent())
        enabled = enabled && node->isInteractionEnabled();
      paint.tint = enabled ? theme().text : theme().mutedText;
    }
    context.drawImage(_raster, _resolved.source, _resolved.destination, paint);
  }
}

Vector::Vector(AssetRegistry &assets, VectorProps props, layout::BoxProps box)
    : Node{box}, _props{std::move(props)}, _assets{assets} {
  validate(_props);
  setHitTestPolicy(HitTestPolicy::None);
  setSemanticProps({.role = SemanticRole::Image});
  _intrinsic = _assets.getVector(_props.source, _props.styles);
  if (!_intrinsic || _intrinsic->w <= 0 || _intrinsic->h <= 0)
    throw std::runtime_error("SVG has no intrinsic area");
}

void Vector::setProps(VectorProps props) {
  validate(props);
  if (_props == props)
    return;
  const bool changedAsset =
      props.source != _props.source || props.styles != _props.styles;
  auto intrinsic =
      changedAsset ? _assets.getVector(props.source, props.styles) : _intrinsic;
  if (!intrinsic || intrinsic->w <= 0 || intrinsic->h <= 0)
    throw std::runtime_error("SVG has no intrinsic area");
  const bool geometry =
      changedAsset || _props.intrinsicSize != props.intrinsicSize;
  _props = std::move(props);
  _intrinsic = std::move(intrinsic);
  if (changedAsset) {
    _source.reset();
    _raster.reset();
    _rasterSize = {};
  }
  _prepared = false;
  if (geometry)
    invalidateLayout();
  else
    invalidatePaint();
}

void Vector::applyPatch(const VectorPatch &patch) {
  const VectorProps defaults{};
  setProps(
      {patch.source.appliedTo(_props.source),
       patch.styles.appliedTo(_props.styles, {}),
       patch.intrinsicSize.appliedTo(_props.intrinsicSize,
                                     defaults.intrinsicSize),
       patch.content.appliedTo(_props.content, defaults.content),
       patch.rasterScale.appliedTo(_props.rasterScale, defaults.rasterScale),
       patch.maximumRasterPixels.appliedTo(_props.maximumRasterPixels,
                                           defaults.maximumRasterPixels),
       patch.useTheme.appliedTo(_props.useTheme, defaults.useTheme)});
}

} // namespace playground::ui
