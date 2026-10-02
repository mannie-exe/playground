#pragma once
#include <ui/Node.hpp>

namespace playground::ui {
struct MeterProps {
  std::optional<double> value;
  double minimum{}, maximum{100};
  std::optional<double> warning, critical;
  std::string name, unit;
};

class Meter final : public Node {
  MeterProps _props;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void paint(PaintContext &) const override;

public:
  explicit Meter(MeterProps = {}, layout::BoxProps = {});
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
