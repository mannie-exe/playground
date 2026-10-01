#pragma once
#include <ui/controls/Composite.hpp>

namespace playground::ui {
enum class ChoiceCenter { Dropdown, Readout };

class ChoiceStepper : public HStack {
  Select *_select{};
  ListBox *_list{};
  Node *_center{};
  Button *_previous{}, *_next{};
  bool _wrap;
  CollectionNavigation _navigation;
  std::vector<NavigationItem> _items;
  std::vector<Connection> _connections;
  Signal<std::string> _changed;
  Signal<std::string, ActionSource> _edited;
  void refresh();

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  ChoiceStepper(std::unique_ptr<Node> display, std::vector<ChoiceItem>,
                SelectionProps = {}, ChoiceCenter = ChoiceCenter::Dropdown,
                bool wrap = false, layout::BoxProps = {}, ControlMetrics = {});
  const SelectionProps &selectionProps() const;
  void setSelectionProps(SelectionProps);
  void stepBy(int, ActionSource = ActionSource::Program);

  Node &focusTarget() noexcept override { return _center->focusTarget(); }

  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection
  onSelectionChanged(support::MoveOnlyFunction<void(std::string)> f) {
    return _changed.connect(std::move(f));
  }

  Connection onSelectionEdited(
      support::MoveOnlyFunction<void(std::string, ActionSource)> f) {
    return _edited.connect(std::move(f));
  }
};
} // namespace playground::ui
