#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include <ui/containers/Box.hpp>

namespace playground::ui {

struct ButtonProps {
  bool enabled{true};
  std::optional<math::ColorRGBA8> normal, hover, pressed, disabled, focus;
  std::optional<float> focusWidth;
  void validate() const;
  bool operator==(const ButtonProps &) const = default;
};

struct ButtonPatch {
  Patch<bool> enabled;
  Patch<std::optional<math::ColorRGBA8>> normal, hover, pressed, disabled;
  Patch<std::optional<math::ColorRGBA8>> focus;
  Patch<std::optional<float>> focusWidth;
};

class Button : public Box {
  ButtonProps _props;
  MotionValue<math::ColorRGBA8> _background{{}, [this] { invalidatePaint(); }};
  AnimationHandle _feedback;
  math::ColorRGBA8 backgroundColor() const;
  void synchronizeBackground();
  Signal<> _activated;
  Signal<ActionSource> _invoked;

  bool _hovered{};
  std::optional<std::uint64_t> _pointer;
  int _button{};
  std::optional<Key> _key;
  ActionSource _keySource{ActionSource::Keyboard};

  void cancel() noexcept;

protected:
  void paint(PaintContext &context) const override;
  void paintSubtree(PaintContext &context) const override;

  void onAttach(UIServices &) override { _background.set(backgroundColor()); }

  void onPropsChanged(const ChangeSet &) override { synchronizeBackground(); }

  void onThemeChanged() noexcept override {
    _feedback.cancel();
    _background.set(services() ? backgroundColor() : theme().elevated);
  }

  void onDetach() noexcept override {
    _feedback.cancel();
    cancel();
    _hovered = false;
  }

  void onDefaultEvent(UIEvent &event) override;

public:
  explicit Button(std::unique_ptr<Node> content = {}, ButtonProps props = {},
                  layout::BoxProps box = {});

  const ButtonProps &buttonProps() const noexcept { return _props; }

  bool isEnabled() const noexcept { return _props.enabled; }

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  bool isHovered() const noexcept { return _hovered; }

  bool isPressed() const noexcept {
    return _pointer.has_value() || _key.has_value();
  }

  void setButtonProps(ButtonProps value);
  void setEnabled(bool value);
  void applyButtonPatch(const ButtonPatch &p);

  Connection onInvoke(support::MoveOnlyFunction<void(ActionSource)> callback) {
    return _invoked.connect(std::move(callback));
  }

  Connection onActivate(support::MoveOnlyFunction<void()> callback) {
    return _activated.connect(std::move(callback));
  }
};

} // namespace playground::ui
