#pragma once

#include <stdexcept>

namespace playground::ui {
enum class AccessibilityMode { Auto, Enabled, Disabled };

struct InteractionProps {
  AccessibilityMode accessibility{AccessibilityMode::Auto};
  bool sequentialNavigation{true};
  bool directionalNavigation{true};

  void validate() const {
    if (accessibility < AccessibilityMode::Auto ||
        accessibility > AccessibilityMode::Disabled)
      throw std::invalid_argument("Invalid accessibility mode");
  }

  bool operator==(const InteractionProps &) const = default;
};
} // namespace playground::ui
