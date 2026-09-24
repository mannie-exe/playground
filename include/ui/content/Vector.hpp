#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <support/AssetRegistry.hpp>
#include <ui/Node.hpp>
#include <ui/Patch.hpp>
#include <ui/content/ContentTypes.hpp>

namespace playground::ui {

struct VectorProps {
  VectorSource source{std::string{}};
  SVGStyleOverrides styles;
  std::optional<math::Size2> intrinsicSize;
  ContentStyle content;
  float rasterScale{1};
  std::size_t maximumRasterPixels{16 * 1024 * 1024};

  bool operator==(const VectorProps &) const = default;
};

struct VectorPatch {
  Patch<VectorSource> source;
  Patch<SVGStyleOverrides> styles;
  Patch<std::optional<math::Size2>> intrinsicSize;
  Patch<ContentStyle> content;
  Patch<float> rasterScale;
  Patch<std::size_t> maximumRasterPixels;
};

class Vector final : public Node {
  VectorProps _props;

  AssetRegistry &_assets;
  SurfaceHandle _intrinsic;
  layout::LayoutDirection _direction{layout::LayoutDirection::LeftToRight};

  rendering::PaintImageHandle _source;
  rendering::PaintImageHandle _raster;
  math::Vec2i _rasterSize{};
  ResolvedContent _resolved{};
  bool _prepared{};

  static void validate(const VectorProps &props);

  math::Size2 naturalSize() const noexcept {
    return _props.intrinsicSize.value_or(math::Size2{
        static_cast<float>(_intrinsic->w), static_cast<float>(_intrinsic->h)});
  }

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {.size = naturalSize()};
  }

  void arrangeChildren(ArrangeContext &context, math::Rect) override {
    _direction = context.direction;
    _prepared = false;
  }

  void prepareContent(PrepareContext &context) override;

  void paint(PaintContext &context) const override;

public:
  Vector(AssetRegistry &assets, VectorProps props, layout::BoxProps box = {});

  const VectorProps &props() const noexcept { return _props; }
  math::Vec2i rasterSize() const noexcept { return _rasterSize; }
  void setProps(VectorProps props);
  void applyPatch(const VectorPatch &patch);
};

} // namespace playground::ui
