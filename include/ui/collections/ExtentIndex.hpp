#pragma once

#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>

namespace playground::ui::detail {

// Fenwick prefix sums: rebuild O(n), update/prefix/lower bound O(log n).
class ExtentIndex {
  std::vector<double> _values;
  std::vector<double> _tree{0.0};

public:
  ExtentIndex() = default;

  explicit ExtentIndex(std::span<const double> values) { reset(values); }

  void reset(std::span<const double> values) {
    std::vector<double> tree(values.size() + 1, 0.0);
    for (std::size_t i = 1; i < tree.size(); ++i) {
      if (!std::isfinite(values[i - 1]) || values[i - 1] < 0)
        throw std::invalid_argument(
            "Extent index requires finite nonnegative extents");
      tree[i] += values[i - 1];
      const auto parent = i + (i & (~i + 1));
      if (parent < tree.size())
        tree[parent] += tree[i];
    }
    _values.assign(values.begin(), values.end());
    _tree = std::move(tree);
  }

  std::size_t size() const noexcept { return _values.size(); }

  double value(std::size_t index) const { return _values.at(index); }

  double prefix(std::size_t count) const {
    if (count > size())
      throw std::out_of_range("Extent prefix outside collection");
    double result{};
    for (; count; count -= count & (~count + 1))
      result += _tree[count];
    return result;
  }

  double total() const { return prefix(size()); }

  void set(std::size_t index, double value) {
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument("Extent must be finite and nonnegative");
    const double delta = value - _values.at(index);
    _values[index] = value;
    for (++index; index < _tree.size(); index += index & (~index + 1))
      _tree[index] += delta;
  }

  // First item whose end is after position, or size() beyond the collection.
  std::size_t itemAt(double position) const {
    if (!std::isfinite(position))
      throw std::invalid_argument("Extent query must be finite");
    if (position < 0)
      return 0;
    std::size_t index{};
    double sum{};
    for (std::size_t bit = std::bit_floor(size()); bit; bit >>= 1) {
      const auto next = index + bit;
      if (next <= size() && sum + _tree[next] <= position) {
        sum += _tree[next];
        index = next;
      }
    }
    return index;
  }
};
} // namespace playground::ui::detail
