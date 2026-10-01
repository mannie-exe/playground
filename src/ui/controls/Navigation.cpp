#include <algorithm>

#include <unicode/unistr.h>

#include <ui/controls/Navigation.hpp>

namespace playground::ui {
std::optional<std::string>
CollectionNavigation::next(std::span<const NavigationItem> items,
                           std::optional<std::string> current, int direction,
                           bool wrap) const {
  if (items.empty() || !direction)
    return {};
  auto found = std::find_if(items.begin(), items.end(),
                            [&](auto &v) { return current == v.key; });
  auto index = found == items.end() ? (direction > 0 ? -1 : int(items.size()))
                                    : int(found - items.begin());
  for (std::size_t n = 0; n < items.size(); ++n) {
    index += direction > 0 ? 1 : -1;
    if (index < 0 || index >= int(items.size())) {
      if (!wrap)
        return {};
      index = (index + int(items.size())) % int(items.size());
    }
    if (items[index].enabled)
      return items[index].key;
  }
  return {};
}

std::optional<std::string> CollectionNavigation::navigate(
    std::span<const NavigationItem> items, std::optional<std::string> current,
    const UIEvent &e, double now, NavigationPolicy policy) {
  if (e.type == EventType::TextInput && !e.text.empty() && !e.control &&
      !e.command) {
    if (now - _lastSearch > 1 || now < _lastSearch)
      _search.clear();
    _lastSearch = now;
    _search += e.text;
    auto needle = icu::UnicodeString::fromUTF8(_search);
    needle.foldCase();
    for (std::size_t n = 0; n < items.size(); ++n) {
      current = next(items, current, 1, true);
      if (!current)
        break;
      auto item = std::find_if(items.begin(), items.end(),
                               [&](auto &v) { return v.key == current; });
      auto label = icu::UnicodeString::fromUTF8(item->label);
      label.foldCase();
      if (label.startsWith(needle))
        return current;
    }
    return {};
  }
  if (e.type != EventType::KeyDown)
    return {};
  const int forward =
      policy.axis == layout::Axis::Horizontal &&
              policy.direction == layout::LayoutDirection::RightToLeft
          ? -1
          : 1;
  if (e.logicalKey == Key::Home)
    return next(items, {}, 1, false);
  if (e.logicalKey == Key::End)
    return next(items, {}, -1, false);
  if (e.logicalKey ==
      (policy.axis == layout::Axis::Vertical ? Key::Down : Key::Right))
    return next(items, current, forward, policy.wrap);
  if (e.logicalKey ==
      (policy.axis == layout::Axis::Vertical ? Key::Up : Key::Left))
    return next(items, current, -forward, policy.wrap);
  return {};
}
} // namespace playground::ui
