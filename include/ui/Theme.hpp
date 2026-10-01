#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>

#include <math/Color.hpp>

namespace playground::ui {
enum class ColorSchemePreference { System, Light, Dark };
enum class ContrastPreference { System, Normal, High };

struct ControlMetrics {
  float minimumHeight{40}, buttonWidth{40}, gap{8};

  void validate() const {
    if (!std::isfinite(minimumHeight) || !std::isfinite(buttonWidth) ||
        !std::isfinite(gap) || minimumHeight <= 0 || buttonWidth <= 0 ||
        gap < 0)
      throw std::invalid_argument("Invalid control metrics");
  }

  bool operator==(const ControlMetrics &) const = default;
};

struct ThemePalette {
  math::ColorRGBA8 surface, elevated, text, mutedText, border, accent, onAccent,
      hover, pressed, focus, selection;
  bool highContrast{};
  math::ColorRGBA8 error{180, 35, 35, 255}, warning{130, 85, 0, 255},
      success{20, 110, 55, 255};
  bool operator==(const ThemePalette &) const = default;
};

struct SystemAppearance {
  std::optional<bool> dark;
  std::optional<bool> highContrast;
  std::optional<ThemePalette> contrastPalette;
};

ThemePalette resolveTheme(ColorSchemePreference, ContrastPreference,
                          const SystemAppearance &);
const ThemePalette &defaultTheme();
} // namespace playground::ui
