#pragma once

#include <cstdint>
#include <limits>

namespace playground::ui {
struct NodeId {
  std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
  std::uint64_t generation{};
  bool operator==(const NodeId &) const = default;
};
} // namespace playground::ui
