#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <ui/containers/AnchorLayout.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/containers/Flow.hpp>
#include <ui/containers/Grid.hpp>
#include <ui/containers/ZStack.hpp>
#include <ui/controls/Button.hpp>
#include <utility>

namespace playground::ui {

template <typename Placement> struct ChildEntry {
  std::unique_ptr<Node> node;
  Placement placementInParent;
};
template <std::derived_from<Node> T, typename... Args>
std::unique_ptr<T> make(Args &&...args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename... Children> auto children(Children &&...values) {
  return std::make_tuple(std::forward<Children>(values)...);
}
template <std::derived_from<Node> T>
auto stackChild(layout::StackPlacement placement, std::unique_ptr<T> child) {
  return ChildEntry<layout::StackPlacement>{std::move(child), placement};
}
template <std::derived_from<Node> T>
auto gridChild(layout::GridPlacement placement, std::unique_ptr<T> child) {
  return ChildEntry<layout::GridPlacement>{std::move(child), placement};
}
template <std::derived_from<Node> T>
auto layerChild(layout::LayerPlacement placement, std::unique_ptr<T> child) {
  return ChildEntry<layout::LayerPlacement>{std::move(child), placement};
}
template <std::derived_from<Node> T>
auto anchorChild(layout::AnchorPlacement placement, std::unique_ptr<T> child) {
  return ChildEntry<layout::AnchorPlacement>{std::move(child), placement};
}

namespace builder_detail {
template <typename Parent, std::derived_from<Node> T>
void append(Parent &parent, std::unique_ptr<T> child) {
  parent.append(std::move(child));
}
template <typename Parent, typename Placement>
void append(Parent &parent, ChildEntry<Placement> child) {
  parent.append(std::move(child.node), std::move(child.placementInParent));
}
template <typename Parent, typename Props, typename Tuple>
auto container(Props props, Tuple values, layout::BoxProps box) {
  auto result = make<Parent>(std::move(props), box);
  std::apply([&](auto &&...item) { (append(*result, std::move(item)), ...); },
             std::move(values));
  return result;
}
} // namespace builder_detail
template <typename... T>
auto makeHStack(layout::StackProps props, std::tuple<T...> values,
                layout::BoxProps box = {}) {
  return builder_detail::container<HStack>(props, std::move(values), box);
}
template <typename... T>
auto makeVStack(layout::StackProps props, std::tuple<T...> values,
                layout::BoxProps box = {}) {
  return builder_detail::container<VStack>(props, std::move(values), box);
}
template <typename... T>
auto makeGrid(layout::GridProps props, std::tuple<T...> values,
              layout::BoxProps box = {}) {
  return builder_detail::container<Grid>(std::move(props), std::move(values),
                                         box);
}
template <typename... T>
auto makeZStack(ZStackProps props, std::tuple<T...> values,
                layout::BoxProps box = {}) {
  return builder_detail::container<ZStack>(props, std::move(values), box);
}
template <typename... T>
auto makeFlow(layout::FlowProps props, std::tuple<T...> values,
              layout::BoxProps box = {}) {
  return builder_detail::container<Flow>(props, std::move(values), box);
}
inline auto makeBox(layout::BoxProps props, std::unique_ptr<Node> content,
                    BoxContentProps alignment = {}) {
  auto result = make<Box>(props, alignment);
  if (content)
    result->setChild(std::move(content));
  return result;
}

} // namespace playground::ui
