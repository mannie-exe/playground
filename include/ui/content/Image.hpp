#pragma once

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <ui/Node.hpp>
#include <ui/Patch.hpp>
#include <ui/content/ContentTypes.hpp>

namespace playground::ui {

struct ImageProps {
  PaintImageHandle image;
  std::optional<math::Rect> sourceRect;
  float assetDensity{1};
  ContentStyle content;

  bool operator==(const ImageProps &) const = default;
};

struct ImagePatch {
  Patch<PaintImageHandle> image;
  Patch<std::optional<math::Rect>> sourceRect;
  Patch<float> assetDensity;
  Patch<ContentStyle> content;
};

class Image final : public Node {
  ImageProps _props;

  PaintImageHandle _image;
  bool _prepared{};
  layout::LayoutDirection _direction{layout::LayoutDirection::LeftToRight};

  static void validate(const ImageProps &props);

  math::Rect source() const noexcept {
    return _props.sourceRect.value_or(
        math::Rect{{}, _props.image->pixelSize()});
  }

protected:
  void prepareContent(PrepareContext &context) override {
    _prepared = false;
    _image = rendering::prepareImage(_props.image, context.images);
    _prepared = true;
  }
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {.size = source().size / _props.assetDensity};
  }

  void arrangeChildren(ArrangeContext &context, math::Rect) override {
    _direction = context.direction;
  }

  void paint(PaintContext &context) const override;

public:
  explicit Image(ImageProps props, layout::BoxProps box = {});

  const ImageProps &props() const noexcept { return _props; }
  void setProps(ImageProps props);
  void applyPatch(const ImagePatch &patch);
};

} // namespace playground::ui
