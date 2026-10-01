#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <ui/containers/Box.hpp>

namespace playground::ui {

struct ButtonProps {
  bool enabled{true};
  math::ColorRGBA8 normal{70, 70, 70, 255};
  math::ColorRGBA8 hover{95, 95, 95, 255};
  math::ColorRGBA8 pressed{45, 45, 45, 255};
  math::ColorRGBA8 disabled{60, 60, 60, 255};
  math::ColorRGBA8 focus{255, 215, 80, 255};
  float focusWidth{2};
  bool useTheme{true};
  void validate() const;
  bool operator==(const ButtonProps &) const = default;
};

struct ButtonPatch {
  Patch<bool> enabled;
  Patch<math::ColorRGBA8> normal, hover, pressed, disabled;
  Patch<math::ColorRGBA8> focus;
  Patch<float> focusWidth;
  Patch<bool> useTheme;
};

class Button : public Box {
  ButtonProps _props;
  Signal<> _activated;

  bool _hovered{};
  std::optional<std::uint64_t> _pointer;
  int _button{};
  std::optional<Key> _key;
  ActionSource _keySource{ActionSource::Keyboard};

  void cancel() noexcept;

protected:
  void paint(PaintContext &context) const override;
  void paintSubtree(PaintContext &context) const override;

  void onDetach() noexcept override {
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

  Connection onActivate(support::MoveOnlyFunction<void()> callback) {
    return _activated.connect(std::move(callback));
  }
};

} // namespace playground::ui
