#pragma once

#include <ui/Node.hpp>
#include <ui/controls/Editing.hpp>

namespace playground::ui {
struct SliderProps {
  RangeValue range;
  bool enabled{true};
  bool readOnly{};
  layout::Axis axis{layout::Axis::Horizontal};
  std::string name;
  math::ColorRGBA8 track{65, 65, 65, 255}, thumb{230, 190, 70, 255};
  bool useTheme{true};
  bool snapToStep{true};
  bool operator==(const SliderProps &) const = default;
};

struct SliderPatch {
  Patch<RangeValue> range;
  Patch<bool> enabled, readOnly;
  Patch<layout::Axis> axis;
  Patch<std::string> name;
  Patch<math::ColorRGBA8> track, thumb;
  Patch<bool> useTheme;
  Patch<bool> snapToStep;
};

class Slider final : public Node {
  SliderProps _props;
  bool _hovered{};
  std::optional<std::uint64_t> _pointer;
  Signal<double> _changed;
  Signal<double, ChangeContext> _edited;
  Signal<ChangeContext> _finished;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void paint(PaintContext &) const override;
  void onDefaultEvent(UIEvent &) override;

  void onDetach() noexcept override {
    _hovered = false;
    _pointer.reset();
    releaseAllPointers();
  }

public:
  explicit Slider(SliderProps props = {}, layout::BoxProps box = {});

  const SliderProps &props() const noexcept { return _props; }

  bool isHovered() const noexcept { return _hovered; }

  bool isDragging() const noexcept { return _pointer.has_value(); }

  void setProps(SliderProps);
  void applyPatch(const SliderPatch &);

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection
  onValueEdited(support::MoveOnlyFunction<void(double, ChangeContext)> f) {
    return _edited.connect(std::move(f));
  }

  Connection
  onInteractionFinished(support::MoveOnlyFunction<void(ChangeContext)> f) {
    return _finished.connect(std::move(f));
  }

  Connection onValueChanged(support::MoveOnlyFunction<void(double)> callback) {
    return _changed.connect(std::move(callback));
  }
};

struct ProgressProps {
  RangeValue range;
  std::string name;
  math::ColorRGBA8 track{50, 50, 50, 255}, fill{90, 190, 100, 255};
  bool useTheme{true};
  bool indeterminate{};
};

class ProgressBar final : public Node {
  ProgressProps _props;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void paint(PaintContext &) const override;

public:
  explicit ProgressBar(ProgressProps props = {}, layout::BoxProps box = {});

  const ProgressProps &props() const noexcept { return _props; }

  void setProps(ProgressProps);
  SemanticState semanticState() const override;
};
} // namespace playground::ui
