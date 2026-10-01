#pragma once

#include <optional>
#include <string>
#include <vector>

#include <ui/TextEdit.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>
#include <ui/controls/Navigation.hpp>

namespace playground::ui {
struct ToggleProps {
  CheckState checked{CheckState::Off};
  bool allowMixed{};
  std::string name;
  bool operator==(const ToggleProps &) const = default;
};

struct TogglePatch {
  Patch<CheckState> checked;
  Patch<bool> allowMixed;
  Patch<std::string> name;
};

class ToggleButton : public Button {
  ToggleProps _props;
  SemanticRole _role;
  Signal<CheckState> _changed;
  Signal<CheckState, ActionSource> _edited;

protected:
  void paint(PaintContext &) const override;

public:
  ToggleButton(std::unique_ptr<Node> content, ToggleProps props = {},
               ButtonProps button = {}, layout::BoxProps box = {},
               SemanticRole role = SemanticRole::Button);

  const ToggleProps &props() const noexcept { return _props; }

  void setProps(ToggleProps);
  void applyPatch(const TogglePatch &);
  SemanticState semanticState() const override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection
  onValueEdited(support::MoveOnlyFunction<void(CheckState, ActionSource)> f) {
    return _edited.connect(std::move(f));
  }

  Connection
  onValueChanged(support::MoveOnlyFunction<void(CheckState)> callback) {
    return _changed.connect(std::move(callback));
  }
};

class Checkbox final : public ToggleButton {
public:
  Checkbox(std::unique_ptr<Node> content, ToggleProps props = {},
           ButtonProps button = {}, layout::BoxProps box = {})
      : ToggleButton{std::move(content), std::move(props), button, box,
                     SemanticRole::Checkbox} {}
};

class Switch final : public ToggleButton {
public:
  Switch(std::unique_ptr<Node> content, ToggleProps props = {},
         ButtonProps button = {}, layout::BoxProps box = {})
      : ToggleButton{std::move(content), std::move(props), button, box,
                     SemanticRole::Switch} {}
};

struct ChoiceItem {
  std::string key;
  std::string label;
  std::unique_ptr<Node> content;
  bool enabled{true};
};

struct SelectionProps {
  std::optional<std::string> selected;
  bool enabled{true};
  bool required{};
  std::string name;
  bool operator==(const SelectionProps &) const = default;
};

struct SelectionPatch {
  Patch<std::optional<std::string>> selected;
  Patch<bool> enabled, required;
  Patch<std::string> name;
};

// Bounded single-selection list. Item keys, not child addresses, are authored
// IDs.
class ListBox : public VStack, public TextInputClient {
  class Option;
  SelectionProps _props;
  SemanticRole _role;
  bool _composing{};
  std::optional<std::string> _active;

  struct Entry {
    std::string key, label;
    Option *button;
    bool enabled;
  };

  std::vector<Entry> _items;
  std::vector<Connection> _connections;
  Signal<std::string> _changed;
  Signal<std::string, ActionSource> _selected;
  Signal<std::string, ActionSource> _invoked;

protected:
  CollectionNavigation _navigation;
  bool handleComposition(UIEvent &);
  void onDefaultEvent(UIEvent &) override;
  std::vector<std::string> enabledKeys() const;
  void highlight(const std::optional<std::string> &);

  Connection
  onCommand(support::MoveOnlyFunction<void(std::string, ActionSource)> f) {
    return _invoked.connect(std::move(f));
  }

public:
  ListBox(std::vector<ChoiceItem> items, SelectionProps props = {},
          ButtonProps button = {}, layout::BoxProps box = {},
          SemanticRole role = SemanticRole::ListBox);

  const SelectionProps &selectionProps() const noexcept { return _props; }

  std::vector<NavigationItem> items() const;
  void setSelectionProps(SelectionProps);
  void applySelectionPatch(const SelectionPatch &);
  SemanticState semanticState() const override;

  TextInputState textInputState() const override {
    return {.readOnly = !_props.enabled,
            .caret = {{}, bounds().size},
            .composing = _composing};
  }

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection onSelectionEdited(
      support::MoveOnlyFunction<void(std::string, ActionSource)> f) {
    return _selected.connect(std::move(f));
  }

  Connection
  onSelectionChanged(support::MoveOnlyFunction<void(std::string)> callback) {
    return _changed.connect(std::move(callback));
  }
};

class RadioGroup final : public ListBox {
public:
  RadioGroup(std::vector<ChoiceItem> items, SelectionProps props = {},
             ButtonProps button = {}, layout::BoxProps box = {})
      : ListBox{std::move(items), std::move(props), button, box,
                SemanticRole::RadioGroup} {}
};

class MenuList final : public ListBox {
  using ListBox::onSelectionChanged;
  using ListBox::onSelectionEdited;

public:
  Connection
  onInvoked(support::MoveOnlyFunction<void(std::string, ActionSource)> f) {
    return onCommand(std::move(f));
  }

  MenuList(std::vector<ChoiceItem> items, SelectionProps props = {},
           ButtonProps button = {}, layout::BoxProps box = {})
      : ListBox{std::move(items), std::move(props), button, box,
                SemanticRole::Menu} {}
};
} // namespace playground::ui
