#pragma once

#include <optional>

#include <math/Color.hpp>

namespace playground::ui {
enum class ColorSchemePreference { System, Light, Dark };
enum class ContrastPreference { System, Normal, High };

struct ThemePalette {
  math::ColorRGBA8 surface, elevated, text, mutedText, border, accent, onAccent,
      hover, pressed, focus, selection;
  bool highContrast{};
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
