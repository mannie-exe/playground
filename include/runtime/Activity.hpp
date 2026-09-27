#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace playground::runtime {

using ActivityClock = std::chrono::steady_clock;

// Update cadence and presentation cadence are independent permissions.
struct ActivityProps {
  bool continuousUpdate{true};
  bool continuousPaint{true};
};

struct ActivityDemand {
  bool update{};
  bool paint{};
  std::optional<ActivityClock::time_point> wakeAt;

  bool updateDue(ActivityClock::time_point now) const noexcept {
    return update || (wakeAt && *wakeAt <= now);
  }
};

// Capture before recording; acknowledge only a successfully submitted frame.
// Requests raised during recording remain pending after acknowledgement.
class PaintRequest {
  std::uint64_t _requested{1}, _submitted{};

public:
  void request() noexcept { ++_requested; }

  bool pending() const noexcept { return _requested != _submitted; }

  auto capture() const noexcept { return _requested; }

  void submitted(std::uint64_t revision) noexcept { _submitted = revision; }
};

} // namespace playground::runtime
