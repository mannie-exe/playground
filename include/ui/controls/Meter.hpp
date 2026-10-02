#pragma once
#include <ui/content/TintableContent.hpp>

namespace playground::ui {
struct MeterProps {
  std::optional<double> value;
  double minimum{}, maximum{100};
  std::optional<double> warning, critical;
  std::string name, unit;
};

class Meter final : public Node {
  MeterProps _props;
  TintableContent *_warningIcon{}, *_criticalIcon{};

  bool critical() const noexcept {
    return _props.value && _props.critical && *_props.value >= *_props.critical;
  }

  bool warning() const noexcept {
    return _props.value && _props.warning && *_props.value >= *_props.warning;
  }

  math::Rect markerBounds() const noexcept;
  math::Rect trackBounds() const noexcept;
  void paintMarker(PaintContext &, math::ColorRGBA8) const;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void paintSubtree(PaintContext &) const override;

public:
  explicit Meter(MeterProps = {}, layout::BoxProps = {});
  Meter(std::unique_ptr<TintableContent> warningIcon,
        std::unique_ptr<TintableContent> criticalIcon, MeterProps = {},
        layout::BoxProps = {});
  void setProps(MeterProps);

  const MeterProps &props() const noexcept { return _props; }

  SemanticState semanticState() const override;
};
enum class ControlGlyph { Minus, Plus, Previous, Next };

class ControlIcon final : public Node {
  ControlGlyph _glyph;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {{themeMetrics().iconSize, themeMetrics().iconSize}};
  }

  void paint(PaintContext &) const override;

public:
  explicit ControlIcon(ControlGlyph glyph) : _glyph{glyph} {
    setSemanticProps({.exposure = SemanticExposure::HiddenSubtree});
  }
};
} // namespace playground::ui
