#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace playground::runtime {

// A host-supplied monotonic clock, independent of SDL and sleeping/waiting.
// Rebase after paused work; the next update starts with a zero delta.
class UpdateClock {
  std::optional<std::uint64_t> _previous;

public:
  double advance(std::uint64_t counter, std::uint64_t frequency) {
    if (!frequency || (_previous && counter < *_previous))
      throw std::invalid_argument("Invalid monotonic update clock sample");
    const auto elapsed = _previous ? static_cast<double>(counter - *_previous) /
                                         static_cast<double>(frequency)
                                   : 0.0;
    _previous = counter;
    return elapsed;
  }
  void rebase() noexcept { _previous.reset(); }
};

} // namespace playground::runtime
