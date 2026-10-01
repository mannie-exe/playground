#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ui/containers/Popup.hpp>
#include <ui/controls/Choice.hpp>

namespace playground::ui {
struct ExpansionProps {
  bool expanded{};
  bool enabled{true};
  std::string name;
  bool operator==(const ExpansionProps &) const = default;
};

struct ExpansionPatch {
  Patch<bool> expanded, enabled;
  Patch<std::string> name;
};

// These are retained in-flow compositions. Overlay placement belongs to the
// author's ZStack/Overlay, not a second hidden native-window hierarchy.
class Disclosure : public VStack {
  class Header;
  ExpansionProps _props;
  Button *_header{};
  Node *_body{};
  Signal<bool> _changed;
  Signal<bool, ActionSource> _edited;

public:
  Disclosure(std::unique_ptr<Node> label, std::unique_ptr<Node> body,
             ExpansionProps props = {}, ButtonProps button = {},
             layout::BoxProps box = {});

  const ExpansionProps &expansionProps() const noexcept { return _props; }

  Node &focusTarget() noexcept override { return *_header; }

  void setExpansionProps(ExpansionProps);
  void applyExpansionPatch(const ExpansionPatch &);
  SemanticState semanticState() const override;

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection onExpandedChanged(support::MoveOnlyFunction<void(bool)> f) {
    return _changed.connect(std::move(f));
  }

  Connection
  onExpandedEdited(support::MoveOnlyFunction<void(bool, ActionSource)> f) {
    return _edited.connect(std::move(f));
  }
};

class Select : public VStack {
  class Trigger;
  class ChoiceList;
  Button *_trigger{};
  ListBox *_list{};
  Popup *_popup{};
  bool _expanded{};
  std::vector<Connection> _connections;
  Signal<std::string> _changed;
  Signal<std::string, ActionSource> _edited;

protected:
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void onDefaultEvent(UIEvent &) override;

public:
  Select(std::unique_ptr<Node> label, std::vector<ChoiceItem> items,
         SelectionProps props = {}, ButtonProps button = {},
         layout::BoxProps box = {});

  const SelectionProps &selectionProps() const noexcept {
    return _list->selectionProps();
  }

  void setSelectionProps(SelectionProps);
  void applySelectionPatch(const SelectionPatch &);

  Node &focusTarget() noexcept override { return *_trigger; }

  bool isExpanded() const noexcept { return _expanded; }

  void setExpanded(bool);

  bool isInteractionEnabled() const noexcept override {
    return selectionProps().enabled;
  }

  SemanticState semanticState() const override;
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

struct TabItem {
  std::string key, name;
  std::unique_ptr<Node> label, panel;
  bool enabled{true};
};

class Tabs : public VStack {
  CollectionNavigation _navigation;
  class Tab;
  SelectionProps _props;

  struct Entry {
    std::string key;
    Button *tab;
    Node *panel;
    bool enabled;
  };

  std::vector<Entry> _items;
  std::vector<Connection> _connections;
  Signal<std::string> _changed;
  Signal<std::string, ActionSource> _edited;

protected:
  void onDefaultEvent(UIEvent &) override;

public:
  Tabs(std::vector<TabItem>, SelectionProps props = {}, ButtonProps button = {},
       layout::BoxProps box = {});

  const SelectionProps &selectionProps() const noexcept { return _props; }

  void setSelectionProps(SelectionProps);
  void applySelectionPatch(const SelectionPatch &);

  bool isInteractionEnabled() const noexcept override { return _props.enabled; }

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

struct DialogProps {
  bool open{};
  bool modal{true};
  bool dismissOnEscape{true};
  std::string name, description;
  std::optional<NodeId> initialFocus, returnFocus;
  bool operator==(const DialogProps &) const = default;
};

struct DialogPatch {
  Patch<bool> open, modal, dismissOnEscape;
  Patch<std::string> name, description;
  Patch<std::optional<NodeId>> initialFocus, returnFocus;
};

class Dialog : public Popup {
  DialogProps _props;
  Signal<> _dismissed;

  // DialogProps is the single public authority for modal presentation.
  using Popup::applyPopupPatch;
  using Popup::setAnchor;
  using Popup::setOpen;
  using Popup::setPopupProps;

protected:
  void onDefaultEvent(UIEvent &) override;
  void paint(PaintContext &) const override;

  virtual bool requiresModal() const noexcept { return false; }

public:
  Dialog(std::unique_ptr<Node>, DialogProps props = {},
         layout::BoxProps box = {});

  const DialogProps &props() const noexcept { return _props; }

  void setProps(DialogProps);
  void applyPatch(const DialogPatch &);
  void dismiss(DismissReason) override;
  ActionResult performAction(const UIAction &, ActionSource) override;

  Connection onDismissed(support::MoveOnlyFunction<void()> f) {
    return _dismissed.connect(std::move(f));
  }
};

struct FieldProps {
  std::string label, description;
  bool operator==(const FieldProps &) const = default;
};

struct FieldPatch {
  Patch<std::string> label, description;
};

class Field : public VStack {
  FieldProps _props;
  SemanticProps _labelBaseline, _descriptionBaseline;
  Node *_control{}, *_label{}, *_description{};
  void link();

protected:
  void onAttach(UIServices &) override;
  void arrangeChildren(ArrangeContext &, math::Rect) override;
  void onDefaultEvent(UIEvent &) override;

public:
  Field(std::unique_ptr<Node> control, std::unique_ptr<Node> label,
        std::unique_ptr<Node> description = {}, FieldProps props = {},
        layout::BoxProps box = {}, bool inlineControl = false);

  const FieldProps &fieldProps() const noexcept { return _props; }

  Node &focusTarget() noexcept override { return _control->focusTarget(); }

  void setFieldProps(FieldProps);
  void applyFieldPatch(const FieldPatch &);

  Node &control() noexcept { return *_control; }
};

class FieldGroup : public VStack {
public:
  explicit FieldGroup(std::string name, layout::StackProps props = {},
                      layout::BoxProps box = {});
};

class Status : public Box {
public:
  Status(std::unique_ptr<Node> content, std::string message = {},
         layout::BoxProps box = {});
  void setMessage(std::string);

  const std::string &message() const noexcept { return semanticProps().name; }
};

class Tooltip : public Popup {
public:
  Tooltip(std::unique_ptr<Node> content, std::string description,
          layout::BoxProps box = {});

  bool isOpen() const noexcept { return popupProps().open; }
};
} // namespace playground::ui
