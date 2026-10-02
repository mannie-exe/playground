#pragma once
#include <ui/controls/Composite.hpp>

namespace playground::ui {
class Popover : public VStack {
  Button *_trigger;
  Popup *_popup;
  std::vector<Connection> _connections;

protected:
  Popup &popup() noexcept { return *_popup; }

  void arrangeChildren(ArrangeContext &, math::Rect) override;

public:
  Popover(std::unique_ptr<Node> trigger, std::unique_ptr<Node> content,
          PopupProps = {}, layout::BoxProps = {});
  void setOpen(bool);

  bool isOpen() const { return _popup->popupProps().open; }

  Node &focusTarget() noexcept override { return *_trigger; }
};

class DropdownMenu : public Popover {
public:
  DropdownMenu(std::unique_ptr<Node> trigger, std::unique_ptr<MenuList>,
               layout::BoxProps = {});

private:
  Connection _invocation;
};

class ContextMenu : public VStack {
  Node *_owner;
  Popup *_popup;
  Connection _invocation, _dismissed;

protected:
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void onDefaultEvent(UIEvent &) override;

public:
  ContextMenu(std::unique_ptr<Node> owner, std::unique_ptr<MenuList>,
              layout::BoxProps = {});
  void openAt(std::optional<math::Point2> position = {});

  Node &focusTarget() noexcept override { return _owner->focusTarget(); }
};

class AlertDialog : public Dialog {
protected:
  bool requiresModal() const noexcept override { return true; }

public:
  AlertDialog(std::unique_ptr<Node> content, DialogProps props = {},
              layout::BoxProps box = {})
      : Dialog{std::move(content),
               [&] {
                 props.modal = true;
                 return props;
               }(),
               box} {}

  SemanticState semanticState() const override {
    auto s = Dialog::semanticState();
    s.description.role = SemanticRole::AlertDialog;
    return s;
  }
};

struct TooltipTiming {
  std::optional<double> showDelay, hideDelay;
};

class TooltipTrigger : public VStack {
  Node *_owner;
  Tooltip *_tooltip;
  TooltipTiming _timing;
  TimerHandle _timer;
  bool _hovered{}, _focused{};
  void schedule();

protected:
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void onDefaultEvent(UIEvent &) override;

  void onDetach() noexcept override {
    _timer.disconnect();
    _hovered = _focused = false;
  }

public:
  TooltipTrigger(std::unique_ptr<Node> owner, std::unique_ptr<Node> help,
                 std::string description = {}, TooltipTiming = {});

  Node &focusTarget() noexcept override { return _owner->focusTarget(); }
};
} // namespace playground::ui
