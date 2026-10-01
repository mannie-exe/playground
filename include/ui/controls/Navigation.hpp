#pragma once
#include <span>

#include <ui/UITypes.hpp>

namespace playground::ui {
struct NavigationItem {
  std::string key, label;
  bool enabled{true};
};

struct NavigationPolicy {
  layout::Axis axis{layout::Axis::Vertical};
  bool wrap{true};
  layout::LayoutDirection direction{layout::LayoutDirection::LeftToRight};
};

class CollectionNavigation {
  std::string _search;
  double _lastSearch{-1};

public:
  void reset() {
    _search.clear();
    _lastSearch = -1;
  }

  std::optional<std::string> next(std::span<const NavigationItem>,
                                  std::optional<std::string>, int direction,
                                  bool wrap) const;
  std::optional<std::string> navigate(std::span<const NavigationItem>,
                                      std::optional<std::string>,
                                      const UIEvent &, double now,
                                      NavigationPolicy = {});
};
} // namespace playground::ui
