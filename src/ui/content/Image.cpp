#include <ui/content/Image.hpp>

namespace playground::ui {

void Image::validate(const ImageProps &props) {
  if (!props.image || !math::isFinite(props.image->pixelSize()) ||
      !math::hasArea(props.image->pixelSize()))
    throw std::invalid_argument("Image requires a nonempty paint image handle");
  if (!std::isfinite(props.assetDensity) || props.assetDensity <= 0)
    throw std::invalid_argument("Image density must be finite and positive");
  content_detail::validate(props.content);
  if (props.sourceRect) {
    const auto source = *props.sourceRect;
    if (!math::isFinite(source) || !math::isNonNegative(source.size) ||
        source.x() < 0 || source.y() < 0 ||
        source.right() > props.image->pixelSize().width ||
        source.bottom() > props.image->pixelSize().height)
      throw std::invalid_argument("Image source rectangle exceeds its surface");
  }
}

void Image::paint(PaintContext &context) const {
  if (!_prepared)
    throw std::logic_error("Image must be prepared after source changes");
  const auto resolved = content_detail::resolve(
      source(), source().size / _props.assetDensity,
      content_detail::contentBounds(bounds(), contentInsets()), _props.content,
      _direction);
  context.drawImage(_image, resolved.source, resolved.destination,
                    _props.content.paint);
}

Image::Image(ImageProps props, layout::BoxProps box)
    : Node{box}, _props{std::move(props)}, _image{_props.image} {
  validate(_props);
  setHitTestPolicy(HitTestPolicy::None);
  setSemanticProps({.role = SemanticRole::Image});
}

void Image::setProps(ImageProps props) {
  validate(props);
  if (_props == props)
    return;
  const bool geometry = _props.image != props.image ||
                        _props.sourceRect != props.sourceRect ||
                        _props.assetDensity != props.assetDensity;
  auto image = props.image;
  _props = std::move(props);
  _image = std::move(image);
  _prepared = false;
  if (geometry)
    invalidateLayout();
  else
    invalidatePaint();
}

void Image::applyPatch(const ImagePatch &patch) {
  const ImageProps defaults{};
  setProps(
      {patch.image.appliedTo(_props.image),
       patch.sourceRect.appliedTo(_props.sourceRect, defaults.sourceRect),
       patch.assetDensity.appliedTo(_props.assetDensity, defaults.assetDensity),
       patch.content.appliedTo(_props.content, defaults.content)});
}

} // namespace playground::ui
