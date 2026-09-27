#pragma once

#include <functional>
#include <memory>
#include <string>

#include <ui/controls/Button.hpp>

namespace playground::ui {

struct StepperProps {
  int value{};
  int minimum{};
  int maximum{100};
  int step{1};
  bool enabled{true};
  std::string name;
  void validate() const;
  bool operator==(const StepperProps &) const = default;
};

struct StepperPatch {
  Patch<int> value, minimum, maximum, step;
  Patch<bool> enabled;
  Patch<std::string> name;
};

// Content is supplied by the caller: core controls do not load fonts or assets.
class Stepper : public Box {
  StepperProps _props;

  Button *_decrease{};
  Button *_increase{};
  Signal<int> _changed;
  Connection _decreaseConnection, _increaseConnection;

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  Stepper(std::unique_ptr<Node> readout, std::unique_ptr<Node> decrease,
          std::unique_ptr<Node> increase, StepperProps props = {},
          ButtonProps buttons = {}, layout::BoxProps box = {});

  const StepperProps &props() const noexcept { return _props; }

  int value() const noexcept { return _props.value; }

  void setProps(StepperProps);
  void applyPatch(const StepperPatch &);
  void stepBy(int direction);
  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  Connection onValueChanged(std::move_only_function<void(int)> callback) {
    return _changed.connect(std::move(callback));
  }
};

} // namespace playground::ui
