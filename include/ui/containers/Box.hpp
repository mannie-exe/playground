#pragma once

#include <functional>
#include <utility>

#include <ui/containers/Container.hpp>

namespace playground::ui {

struct BoxContentProps {
  layout::Alignment contentAlignment;
  bool operator==(const BoxContentProps &) const = default;
};

struct BoxContentPatch {
  Patch<layout::Alignment> contentAlignment;
};

struct BoxPlacementPatch {
  Patch<math::Insets> margin;
  Patch<std::optional<layout::Alignment>> alignmentOverride;
};

class Box : public PlacementContainer<layout::BoxPlacement> {
  BoxContentProps _props;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;

  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit Box(layout::BoxProps box = {}, BoxContentProps props = {})
      : PlacementContainer{box}, _props{props} {}

  const BoxContentProps &contentProps() const noexcept { return _props; }

  void setContentProps(BoxContentProps props);

  void applyContentPatch(const BoxContentPatch &patch) {
    setContentProps(
        {patch.contentAlignment.appliedTo(_props.contentAlignment, {})});
  }

  void setContentAlignment(layout::Alignment alignment) {
    setContentProps({alignment});
  }

  void applyPlacementPatch(const BoxPlacementPatch &patch);

  Node &setChild(std::unique_ptr<Node> child,
                 layout::BoxPlacement placement = {});

  std::unique_ptr<Node> takeChild() {
    return children().empty() ? nullptr : PlacementContainer::takeChild(0);
  }

private:
  using PlacementContainer::append;
  using PlacementContainer::insert;
  using PlacementContainer::moveChild;
  using PlacementContainer::remove;
  using PlacementContainer::reparentFrom;
};

// Components retain their subtree; derived classes own application-specific
// state.
class Component : public Box {
public:
  using Box::Box;

  Node &replaceContent(std::unique_ptr<Node> content) {
    return setChild(std::move(content));
  }
};

struct CustomViewCallbacks {
  std::function<layout::MeasureResult(MeasureContext &,
                                      const layout::SizeConstraints &)>
      measure;
  std::function<void(PaintContext &)> paint;
  std::function<void(UIEvent &)> event;
};

class CustomView : public Node {
  CustomViewCallbacks _callbacks;

protected:
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override {
    return _callbacks.measure ? _callbacks.measure(context, offered)
                              : layout::MeasureResult{};
  }

  void paint(PaintContext &context) const override {
    if (_callbacks.paint)
      _callbacks.paint(context);
  }

  void onEvent(UIEvent &event) override {
    if (_callbacks.event)
      _callbacks.event(event);
  }

public:
  explicit CustomView(CustomViewCallbacks callbacks = {},
                      layout::BoxProps box = {})
      : Node{box}, _callbacks{std::move(callbacks)} {}
};

} // namespace playground::ui
