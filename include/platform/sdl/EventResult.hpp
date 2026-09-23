#pragma once

#include <format>
#include <string_view>

// Result of host/app SDL event dispatch, distinct from a retained UIEvent's
// handled, propagationStopped, and defaultPrevented flags.
enum class EventResult {
  Ignored,
  Handled,
  Consumed,
};

constexpr bool isTerminal(EventResult result) {
  return result == EventResult::Consumed;
}

constexpr EventResult combine(EventResult previous, EventResult next) {
  if (previous == EventResult::Consumed || next == EventResult::Consumed)
    return EventResult::Consumed;
  if (previous == EventResult::Handled || next == EventResult::Handled)
    return EventResult::Handled;
  return EventResult::Ignored;
}

constexpr std::string_view toString(EventResult result) {
  switch (result) {
  case EventResult::Ignored:
    return "Ignored";
  case EventResult::Handled:
    return "Handled";
  case EventResult::Consumed:
    return "Consumed";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<EventResult> : std::formatter<std::string_view> {
  auto format(EventResult result, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(result), ctx);
  }
};
