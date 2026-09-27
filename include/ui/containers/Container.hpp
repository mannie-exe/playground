#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <layout/LayoutAlgorithms.hpp>
#include <ui/Node.hpp>

namespace playground::ui {

// Parent-specific placement is authoritative here, never duplicated on a child.
template <typename Placement> class PlacementContainer : public Node {
  std::vector<Placement> _placements;

protected:
  explicit PlacementContainer(layout::BoxProps box = {}) : Node{box} {
    static_assert(std::is_nothrow_move_constructible_v<Placement>);
    static_assert(std::is_nothrow_move_assignable_v<Placement>);
  }

  virtual void validatePlacement(const Placement &placement) const {
    placement.validate();
  }

  virtual void validatePlacementAt(const Placement &placement,
                                   std::optional<std::size_t>) const {
    validatePlacement(placement);
  }

public:
  std::size_t indexOf(NodeId id) const {
    if (id.index == std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(
          "Child has no attached identity; use its index while constructing");
    for (std::size_t i = 0; i < children().size(); ++i)
      if (children()[i]->id() == id)
        return i;
    throw std::out_of_range("Node is not a child of this container");
  }

  const Placement &placementOf(NodeId id) const {
    return placementInParent(indexOf(id));
  }

  void setPlacement(NodeId id, Placement value) {
    setPlacementInParent(indexOf(id), std::move(value));
  }

  std::unique_ptr<Node> takeChild(NodeId id) { return takeChild(indexOf(id)); }

  void remove(NodeId id) { remove(indexOf(id)); }

  void moveChild(NodeId id, std::size_t to) { moveChild(indexOf(id), to); }

  Node &reparentFrom(PlacementContainer &source, NodeId id,
                     Placement placement = {}) {
    validatePlacementAt(placement, {});
    const auto from = source.indexOf(id);
    _placements.reserve(_placements.size() + 1);
    Node &node = transferChildAt(source, from, children().size());
    source._placements.erase(source._placements.begin() + from);
    _placements.push_back(std::move(placement));
    return node;
  }

  const Placement &placementInParent(std::size_t index) const {
    return _placements.at(index);
  }

  void setPlacementInParent(std::size_t index, Placement placement) {
    checkStructuralMutation();
    validatePlacementAt(placement, index);
    auto &current = _placements.at(index);
    if (current != placement) {
      current = std::move(placement);
      invalidateLayout();
    }
  }

  Node &insert(std::size_t index, std::unique_ptr<Node> child,
               Placement placement = {}) {
    checkStructuralMutation();
    if (index > _placements.size())
      throw std::out_of_range("UI child insertion index");
    validatePlacementAt(placement, {});
    // Allocate metadata before Node changes ownership or attaches the subtree.
    _placements.reserve(_placements.size() + 1);
    Node &result = insertChildAt(index, std::move(child));
    _placements.insert(_placements.begin() + static_cast<std::ptrdiff_t>(index),
                       std::move(placement));
    return result;
  }

  Node &append(std::unique_ptr<Node> child, Placement placement = {}) {
    return insert(_placements.size(), std::move(child), std::move(placement));
  }

  std::unique_ptr<Node> takeChild(std::size_t index) {
    auto child = takeChildAt(index);
    _placements.erase(_placements.begin() + static_cast<std::ptrdiff_t>(index));
    return child;
  }

  void remove(std::size_t index) { (void)takeChild(index); }

  void moveChild(std::size_t from, std::size_t to) {
    moveChildAt(from, to);
    if (from < to)
      std::rotate(_placements.begin() + static_cast<std::ptrdiff_t>(from),
                  _placements.begin() + static_cast<std::ptrdiff_t>(from + 1),
                  _placements.begin() + static_cast<std::ptrdiff_t>(to + 1));
    else if (from > to)
      std::rotate(_placements.begin() + static_cast<std::ptrdiff_t>(to),
                  _placements.begin() + static_cast<std::ptrdiff_t>(from),
                  _placements.begin() + static_cast<std::ptrdiff_t>(from + 1));
  }
};

namespace container_detail {

inline float main(math::Size2 size, layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? size.width : size.height;
}

inline float cross(math::Size2 size, layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? size.height : size.width;
}

inline math::Size2 size(float mainValue, float crossValue, layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? math::Size2{mainValue, crossValue}
                                          : math::Size2{crossValue, mainValue};
}

inline layout::AxisConstraints main(const layout::SizeConstraints &value,
                                    layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? value.width : value.height;
}

inline layout::AxisConstraints cross(const layout::SizeConstraints &value,
                                     layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? value.height : value.width;
}

inline layout::SizeConstraints constraints(layout::AxisConstraints mainValue,
                                           layout::AxisConstraints crossValue,
                                           layout::Axis axis) {
  return axis == layout::Axis::Horizontal
             ? layout::SizeConstraints{mainValue, crossValue}
             : layout::SizeConstraints{crossValue, mainValue};
}

inline float marginMain(math::Insets value, layout::Axis axis) {
  return layout::detail::checked(
      axis == layout::Axis::Horizontal
          ? static_cast<double>(value.left) + value.right
          : static_cast<double>(value.top) + value.bottom);
}

inline float marginCross(math::Insets value, layout::Axis axis) {
  return marginMain(value, axis == layout::Axis::Horizontal
                               ? layout::Axis::Vertical
                               : layout::Axis::Horizontal);
}

inline layout::AxisConstraints limits(const Node &node, layout::Axis axis) {
  const auto &box = node.boxProps();
  return axis == layout::Axis::Horizontal
             ? layout::AxisConstraints{box.minWidth, box.maxWidth}
             : layout::AxisConstraints{box.minHeight, box.maxHeight};
}

inline layout::SizeRule rule(const Node &node, layout::Axis axis) {
  return axis == layout::Axis::Horizontal ? node.boxProps().width
                                          : node.boxProps().height;
}

inline layout::SizeConstraints loose(layout::SizeConstraints value) {
  value.width.minimum = 0;
  value.height.minimum = 0;
  return value;
}

inline layout::Alignment eligibleAlignment(const Node &node,
                                           layout::Alignment alignment) {
  if (alignment.horizontal == layout::Align::Stretch &&
      node.boxProps().width.kind() == layout::SizeKind::Fixed)
    alignment.horizontal = layout::Align::Start;
  if (alignment.vertical == layout::Align::Stretch &&
      node.boxProps().height.kind() == layout::SizeKind::Fixed)
    alignment.vertical = layout::Align::Start;
  return alignment;
}

inline void placeAligned(Node &child, ArrangeContext &context,
                         math::Rect bounds, math::Insets margin,
                         layout::Alignment requested) {
  auto alignment = eligibleAlignment(child, requested);
  auto offered =
      loose(layout::SizeConstraints::tight(bounds.size).deflated(margin));
  if (alignment.horizontal == layout::Align::Stretch)
    offered.width = layout::AxisConstraints::tight(
        limits(child, layout::Axis::Horizontal).clamp(*offered.width.maximum));
  if (alignment.vertical == layout::Align::Stretch)
    offered.height = layout::AxisConstraints::tight(
        limits(child, layout::Axis::Vertical).clamp(*offered.height.maximum));
  const auto measured = child.measure(context, offered);
  // Extents were resolved above with authored limits; alignBounds must not
  // stretch again.
  if (alignment.horizontal == layout::Align::Stretch)
    alignment.horizontal = layout::Align::Start;
  if (alignment.vertical == layout::Align::Stretch)
    alignment.vertical = layout::Align::Start;
  child.arrange(context, layout::alignBounds(bounds, measured.size, alignment,
                                             margin, context.direction));
}

inline float sum(float a, float b) {
  return layout::detail::checked(static_cast<double>(a) + b);
}

} // namespace container_detail
} // namespace playground::ui
