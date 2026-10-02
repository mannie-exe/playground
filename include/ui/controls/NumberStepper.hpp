#pragma once
#include <ui/controls/Button.hpp>
#include <ui/controls/Editing.hpp>

namespace playground::ui {
struct NumberStepperProps {
  double value{}, minimum{}, maximum{100}, step{1};
  bool enabled{true};
  std::string name;
  bool readOnly{};
  void validate() const;
  bool operator==(const NumberStepperProps &) const = default;
};

struct NumberStepperPatch {
  Patch<double> value, minimum, maximum, step;
  Patch<bool> enabled;
  Patch<std::string> name;
  Patch<bool> readOnly;
};

// A NumericEditor center is authoritative; otherwise this owns the readout
// value.
class NumberStepper : public Box {
  NumberStepperProps _props;
  Node *_center{};
  NumericEditor *_editor{};
  Button *_decrease{}, *_increase{};
  Signal<double> _changed;
  Signal<double, ChangeContext> _edited;
  Connection _decreaseConnection, _increaseConnection, _editorConnection;
  Connection _draftConnection;
  void publish(double, ChangeContext);
  void refresh();

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  NumberStepper(std::unique_ptr<Node> center, std::unique_ptr<Node> decrease,
                std::unique_ptr<Node> increase, NumberStepperProps = {},
                ButtonProps = {}, layout::BoxProps = {},
                std::optional<ControlMetrics> = {});

  const NumberStepperProps &props() const noexcept { return _props; }

  double value() const noexcept {
    return _editor ? _editor->acceptedNumber() : _props.value;
  }

  void setProps(NumberStepperProps);
  void applyPatch(const NumberStepperPatch &);
  void stepBy(int, ActionSource = ActionSource::Program);

  Node &focusTarget() noexcept override {
    return _editor ? _center->focusTarget() : *this;
  }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  Connection onValueChanged(support::MoveOnlyFunction<void(double)> f) {
    return _changed.connect(std::move(f));
  }

  Connection
  onValueEdited(support::MoveOnlyFunction<void(double, ChangeContext)> f) {
    return _edited.connect(std::move(f));
  }
};
} // namespace playground::ui
