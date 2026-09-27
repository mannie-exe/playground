#pragma once

#include <optional>
#include <string>
#include <vector>

#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>

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
  onValueChanged(std::move_only_function<void(CheckState)> callback) {
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
class ListBox : public VStack {
  class Option;
  SelectionProps _props;
  SemanticRole _role;

  struct Entry {
    std::string key, label;
    Option *button;
    bool enabled;
  };

  std::vector<Entry> _items;
  std::vector<Connection> _connections;
  Signal<std::string> _changed;

protected:
  void onDefaultEvent(UIEvent &) override;
  std::vector<std::string> enabledKeys() const;
  void highlight(const std::optional<std::string> &);

public:
  ListBox(std::vector<ChoiceItem> items, SelectionProps props = {},
          ButtonProps button = {}, layout::BoxProps box = {},
          SemanticRole role = SemanticRole::ListBox);

  const SelectionProps &selectionProps() const noexcept { return _props; }

  void setSelectionProps(SelectionProps);
  void applySelectionPatch(const SelectionPatch &);
  SemanticState semanticState() const override;

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection
  onSelectionChanged(std::move_only_function<void(std::string)> callback) {
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

class Menu final : public ListBox {
public:
  Menu(std::vector<ChoiceItem> items, SelectionProps props = {},
       ButtonProps button = {}, layout::BoxProps box = {})
      : ListBox{std::move(items), std::move(props), button, box,
                SemanticRole::Menu} {}
};
} // namespace playground::ui
