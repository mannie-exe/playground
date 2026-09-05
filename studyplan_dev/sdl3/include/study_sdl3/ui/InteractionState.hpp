#pragma once

#include <format>
#include <string_view>

enum class InteractionState {
  Normal,
  Hovered,
  Pressed,
  PressedOutside,
  Focused,
  Dragging,
  Dropped,
  Disabled,
};

constexpr std::string_view toString(InteractionState method) {
  switch (method) {
  case InteractionState::Normal:
    return "Normal";
  case InteractionState::Hovered:
    return "Hovered";
  case InteractionState::Pressed:
    return "Pressed";
  case InteractionState::PressedOutside:
    return "PressedOutside";
  case InteractionState::Focused:
    return "Focused";
  case InteractionState::Dragging:
    return "Dragging";
  case InteractionState::Dropped:
    return "Dropped";
  case InteractionState::Disabled:
    return "Disabled";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<InteractionState> : std::formatter<std::string_view> {
  auto format(InteractionState method, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(method), ctx);
  }
};
