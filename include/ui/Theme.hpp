#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <support/FontHandle.hpp>
#include <ui/FontFamily.hpp>

class AssetRegistry;

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
  math::ColorRGBA8 scrollbar{116, 130, 151, 255}, backdrop{0, 0, 0, 100};
  bool operator==(const ThemePalette &) const = default;
};

enum class ColorTreatment { Adaptive, PreserveArtwork };
enum class TextInk { Primary, Secondary, Error, Warning, Success, OnAccent };
enum class TextRole {
  Display,
  Title,
  Heading,
  Body,
  Label,
  Caption,
  Code,
  Value,
  Prose,
  Count
};
enum class FontFamily { Interface, Display, Monospace, Serif, Count };

struct TextStyle {
  FontFamily family{FontFamily::Interface};
  float size{18}, lineHeight{1.25f};
  FontSelection face;
  bool operator==(const TextStyle &) const = default;
};

struct ThemeTypography {
  std::array<TextStyle, static_cast<std::size_t>(TextRole::Count)> styles{
      {{FontFamily::Display, 36, 1.2f, {700}},
       {FontFamily::Display, 28, 1.2f, {700}},
       {FontFamily::Display, 22, 1.25f, {700}},
       {FontFamily::Interface, 18, 1.25f, {}},
       {FontFamily::Display, 18, 1.25f, {}},
       {FontFamily::Interface, 14, 1.3f, {}},
       {FontFamily::Monospace, 16, 1.25f, {}},
       {FontFamily::Interface, 18, 1.25f, {}},
       {FontFamily::Serif, 18, 1.4f, {}}}};
  std::array<FontFamilyHandle, static_cast<std::size_t>(FontFamily::Count)>
      families;
  // Absent inherits the host defaults; an empty list explicitly disables them.
  std::optional<std::vector<FontFamilyHandle>> fallbacks;
  float textScale{1};
  void validate() const;
  bool operator==(const ThemeTypography &) const = default;
};

struct ThemeMetrics {
  ControlMetrics stepper;
  float controlHeight{40}, buttonPadding{10}, padding{10}, gap{8},
      sectionGap{16};
  float fieldGap{6};
  float borderWidth{1}, focusWidth{2}, indicatorStroke{2}, emphasisWidth{3},
      disabledDash{4};
  float indicatorSize{20}, switchWidth{38}, switchHeight{22}, indicatorGap{10};
  float iconSize{16}, chevronWidth{12}, chevronHeight{8};
  float sliderLength{160}, sliderBreadth{24}, sliderTrack{4};
  float sliderThumbLength{16}, sliderThumbBreadth{20};
  float meterLength{160}, meterHeight{16}, meterMarker{12};
  float inputWidth{240}, inputPadding{4}, caretWidth{1}, textAreaLines{5};
  float scrollbarThickness{8}, scrollbarMinimumThumb{16};
  float popupGap{4}, popupPadding{8}, popupMaximumHeight{320};
  float settingsPadding{20};
  double tooltipShowDelay{.5}, tooltipHideDelay{.1};
  void validate() const;
  bool operator==(const ThemeMetrics &) const = default;
};

enum class ControlLayout {
  None,
  Button,
  Choice,
  Checkbox,
  Switch,
  Trigger,
  StepperButton,
  StepperCenter,
  StepperRow,
  FieldRow,
  FieldStack,
  Group,
  Settings,
  Section,
  InputGroup,
  Count
};

struct ControlStyle {
  std::optional<math::Insets> padding;
  std::optional<float> minimumHeight, width, gap;
  void validate() const;
  bool operator==(const ControlStyle &) const = default;
};

struct ResolvedTheme {
  ThemePalette colors;
  ThemeMetrics metrics;
  ThemeTypography typography;
  bool forcedColors{};
  bool operator==(const ResolvedTheme &) const = default;
};

struct ThemeDefinition {
  ThemePalette light, dark, lightHighContrast, darkHighContrast;
  ThemeMetrics metrics;
  ThemeTypography typography;
  void validate() const;
  bool operator==(const ThemeDefinition &) const = default;
};

struct ThemeOverrides {
  std::optional<ThemePalette> colors;
  std::optional<ThemeMetrics> metrics;
  std::optional<ThemeTypography> typography;
  std::optional<ControlMetrics> stepper;
  void validate() const;
  bool operator==(const ThemeOverrides &) const = default;
};

struct SystemAppearance {
  std::optional<bool> dark;
  std::optional<bool> highContrast;
  std::optional<ThemePalette> contrastPalette;
};

ThemePalette resolveTheme(ColorSchemePreference, ContrastPreference,
                          const SystemAppearance &);
const ThemePalette &defaultTheme();
const ThemeDefinition &defaultThemeDefinition();
ResolvedTheme resolveTheme(const ThemeDefinition &, ColorSchemePreference,
                           ContrastPreference, const SystemAppearance &);
const ResolvedTheme &defaultResolvedTheme();
ControlStyle resolveControlStyle(ControlLayout, const ThemeMetrics &,
                                 const ControlStyle &);
FontHandle resolveThemeFont(const ThemeTypography &, std::optional<TextRole>,
                            FontHandle fallback,
                            AssetRegistry *assets = nullptr,
                            std::optional<FontFamily> family = {},
                            std::optional<FontSelection> selection = {});
} // namespace playground::ui
