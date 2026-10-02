#pragma once
#include <set>

#include <ui/containers/Flow.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Navigation.hpp>

namespace playground::ui {
struct ToggleGroupProps {
  std::set<std::string> selected;
  bool multiple{}, required{}, enabled{true};
  std::string name;
};

class ToggleGroup : public Flow {
  ToggleGroupProps _props;
  const bool _checkboxes;
  std::vector<NavigationItem> _items;
  std::vector<ToggleButton *> _buttons;
  std::vector<Connection> _connections;
  CollectionNavigation _navigation;
  Signal<std::set<std::string>, ActionSource> _changed;

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  ToggleGroup(std::vector<ChoiceItem>, ToggleGroupProps = {},
              bool checkboxes = false, layout::BoxProps = {});
  void setProps(ToggleGroupProps);

  const ToggleGroupProps &props() const { return _props; }

  CheckState aggregate() const;
  ActionResult performAction(const UIAction &, ActionSource) override;

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  Connection onSelectionChanged(
      support::MoveOnlyFunction<void(std::set<std::string>, ActionSource)> f) {
    return _changed.connect(std::move(f));
  }
};

class CheckboxGroup final : public ToggleGroup {
public:
  CheckboxGroup(std::vector<ChoiceItem> items, ToggleGroupProps props = {},
                layout::BoxProps box = {})
      : ToggleGroup{std::move(items), std::move(props), true, box} {}
};

class Toolbar : public HStack {
  CollectionNavigation _navigation;
  NodeHandle<Node> _active;
  Node *activeEntry();

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  explicit Toolbar(std::string name = {}, layout::BoxProps box = {});

  Node *sequentialFocusTarget(Node &) override { return activeEntry(); }
};

struct AccordionItem {
  std::string key;
  std::unique_ptr<Node> label, content;
};

class Accordion : public VStack {
  std::vector<std::pair<std::string, Disclosure *>> _items;
  std::vector<Connection> _connections;
  bool _multiple;
  Signal<std::set<std::string>> _changed;
  Signal<std::set<std::string>, ActionSource> _edited;

public:
  Accordion(std::vector<AccordionItem>, bool multiple = false,
            layout::BoxProps = {});
  std::set<std::string> expanded() const;
  void setExpanded(std::set<std::string>);

  Connection
  onExpandedChanged(support::MoveOnlyFunction<void(std::set<std::string>)> f) {
    return _changed.connect(std::move(f));
  }

  Connection onExpandedEdited(
      support::MoveOnlyFunction<void(std::set<std::string>, ActionSource)> f) {
    return _edited.connect(std::move(f));
  }
};
} // namespace playground::ui
