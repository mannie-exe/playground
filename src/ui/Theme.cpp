#include <algorithm>
#include <stdexcept>

#include <ui/Theme.hpp>

namespace playground::ui {
ThemePalette resolveTheme(ColorSchemePreference scheme,
                          ContrastPreference contrast,
                          const SystemAppearance &system) {
  if (scheme < ColorSchemePreference::System ||
      scheme > ColorSchemePreference::Dark ||
      contrast < ContrastPreference::System ||
      contrast > ContrastPreference::High)
    throw std::invalid_argument("Invalid appearance preference");
  const bool dark = scheme == ColorSchemePreference::System
                        ? system.dark.value_or(false)
                        : scheme == ColorSchemePreference::Dark;
  const bool high = contrast == ContrastPreference::System
                        ? system.highContrast.value_or(false)
                        : contrast == ContrastPreference::High;
  if (high && system.highContrast.value_or(false) && system.contrastPalette) {
    auto result = *system.contrastPalette;
    result.highContrast = true;
    return result;
  }
  const auto rgb = [](unsigned value) {
    return math::ColorRGBA8{static_cast<unsigned char>(value >> 16),
                            static_cast<unsigned char>(value >> 8),
                            static_cast<unsigned char>(value), 255};
  };
  ThemePalette result;
  if (high)
    result = {rgb(dark ? 0x000000 : 0xffffff), rgb(dark ? 0x000000 : 0xffffff),
              rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffffff : 0x000000),
              rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffff00 : 0x000080),
              rgb(dark ? 0x000000 : 0xffffff), rgb(dark ? 0x303030 : 0xe0e0e0),
              rgb(dark ? 0x505050 : 0xc0c0c0), rgb(dark ? 0x00ffff : 0x0000aa),
              rgb(dark ? 0x404000 : 0xd0d0ff), true,
              rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffffff : 0x000000),
              rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffffff : 0x000000)};
  else if (dark)
    result = {rgb(0x181c22), rgb(0x252c36), rgb(0xf0f3f7), rgb(0xaeb9c8),
              rgb(0x748297), rgb(0x90bfff), rgb(0x102340), rgb(0x344153),
              rgb(0x435570), rgb(0xffdb80), rgb(0x344d70), false,
              rgb(0xffa8a8), rgb(0xffd080), rgb(0x8ee0a5), rgb(0xaeb9c8)};
  else
    result = {rgb(0xf2f4f7), rgb(0xffffff), rgb(0x17212e), rgb(0x506074),
              rgb(0x687a90), rgb(0x195bb5), rgb(0xffffff), rgb(0xe0e9f5),
              rgb(0xcbdaf0), rgb(0x754400), rgb(0xd4e4fa), false,
              rgb(0xb42323), rgb(0x825500), rgb(0x146e37), rgb(0x687a90)};
  result.onWarning = result.onError =
      high || dark ? result.surface : result.elevated;
  result.meterTrack = high ? result.surface : rgb(dark ? 0x303b49 : 0xdce3ed);
  result.meterOnTrack = result.text;
  return result;
}

const ThemePalette &defaultTheme() {
  static const auto value = resolveTheme(ColorSchemePreference::Light,
                                         ContrastPreference::Normal, {});
  return value;
}

void ThemeTypography::validate() const {
  if (fallbacks)
    for (const auto &family : *fallbacks)
      if (!family)
        throw std::invalid_argument("Null fallback font family");
  for (const auto &style : styles)
    style.face.validate();
  if (!std::isfinite(textScale) || textScale <= 0 || textScale > 8)
    throw std::invalid_argument("Invalid theme text scale");
  for (const auto &style : styles)
    if (style.family < FontFamily::Interface ||
        style.family >= FontFamily::Count || !std::isfinite(style.size) ||
        style.size <= 0 || style.size > 512 ||
        !std::isfinite(style.lineHeight) || style.lineHeight < 1 ||
        style.lineHeight > 4)
      throw std::invalid_argument("Invalid theme text style");
}

void ThemeMetrics::validate() const {
  stepper.validate();
  for (auto value : {controlHeight,
                     buttonPadding,
                     padding,
                     gap,
                     sectionGap,
                     fieldGap,
                     borderWidth,
                     focusWidth,
                     indicatorStroke,
                     emphasisWidth,
                     disabledDash,
                     indicatorSize,
                     switchWidth,
                     switchHeight,
                     indicatorGap,
                     iconSize,
                     chevronWidth,
                     chevronHeight,
                     sliderLength,
                     sliderBreadth,
                     sliderTrack,
                     sliderThumbLength,
                     sliderThumbBreadth,
                     meterLength,
                     meterHeight,
                     meterMarker,
                     meterMarkerInset,
                     inputWidth,
                     inputPadding,
                     caretWidth,
                     textAreaLines,
                     scrollbarThickness,
                     scrollbarMinimumThumb,
                     scrollbarContentGap.value_or(gap),
                     popupGap,
                     popupPadding,
                     popupMaximumHeight,
                     settingsPadding})
    if (!std::isfinite(value) || value < 0 || value > 65536)
      throw std::invalid_argument("Invalid theme metric");
  if (disabledDash <= 0 || emphasisWidth <= 0 || chevronWidth <= 0 ||
      chevronHeight < indicatorStroke || sliderLength <= 0 ||
      sliderBreadth <= 0 || sliderTrack <= 0 || sliderThumbLength <= 0 ||
      sliderThumbBreadth <= 0 || meterLength <= 0 || meterHeight <= 0 ||
      meterMarker <= 0 || focusWidth < 1 || indicatorStroke <= 0 ||
      indicatorSize <= 0 || switchHeight <= 0 || switchWidth < switchHeight ||
      iconSize <= 0 || inputWidth <= 0 || textAreaLines < 1 ||
      caretWidth <= 0 || popupMaximumHeight <= 0 ||
      !std::isfinite(tooltipShowDelay) || tooltipShowDelay < 0 ||
      !std::isfinite(tooltipHideDelay) || tooltipHideDelay < 0)
    throw std::invalid_argument("Invalid theme control geometry/timing");
}

void ControlStyle::validate() const {
  if (padding)
    for (auto v :
         {padding->left, padding->top, padding->right, padding->bottom})
      if (!std::isfinite(v) || v < 0)
        throw std::invalid_argument("Invalid control padding");
  for (auto v : {minimumHeight, width, gap})
    if (v && (!std::isfinite(*v) || *v < 0))
      throw std::invalid_argument("Invalid control style");
}

void ThemeDefinition::validate() const {
  motion.validate();
  metrics.validate();
  typography.validate();
}

void ThemeOverrides::validate() const {
  if (motion)
    motion->validate();
  if (metrics)
    metrics->validate();
  if (typography)
    typography->validate();
  if (stepper)
    stepper->validate();
}

const ThemeDefinition &defaultThemeDefinition() {
  static const ThemeDefinition value{
      resolveTheme(ColorSchemePreference::Light, ContrastPreference::Normal,
                   {}),
      resolveTheme(ColorSchemePreference::Dark, ContrastPreference::Normal, {}),
      resolveTheme(ColorSchemePreference::Light, ContrastPreference::High, {}),
      resolveTheme(ColorSchemePreference::Dark, ContrastPreference::High, {})};
  return value;
}

ResolvedTheme resolveTheme(const ThemeDefinition &definition,
                           ColorSchemePreference scheme,
                           ContrastPreference contrast,
                           const SystemAppearance &system) {
  definition.validate();
  // Retain preference validation in the palette convenience API.
  (void)resolveTheme(scheme, contrast, {});
  const bool dark =
      scheme == ColorSchemePreference::Dark ||
      (scheme == ColorSchemePreference::System && system.dark.value_or(false));
  const bool high = contrast == ContrastPreference::High ||
                    (contrast == ContrastPreference::System &&
                     system.highContrast.value_or(false));
  const bool forced = high && system.highContrast.value_or(false) &&
                      system.contrastPalette.has_value();
  auto colors = forced ? *system.contrastPalette
                : high ? (dark ? definition.darkHighContrast
                               : definition.lightHighContrast)
                       : (dark ? definition.dark : definition.light);
  colors.highContrast = high;
  auto metrics = definition.metrics;
  if (high) {
    metrics.borderWidth = std::max(2.f, metrics.borderWidth);
    metrics.focusWidth = std::max(2.f, metrics.focusWidth);
  }
  return {colors, metrics, definition.typography, forced, definition.motion};
}

const ResolvedTheme &defaultResolvedTheme() {
  static const auto value =
      resolveTheme(defaultThemeDefinition(), ColorSchemePreference::Light,
                   ContrastPreference::Normal, {});
  return value;
}

ControlStyle resolveControlStyle(ControlLayout role, const ThemeMetrics &m,
                                 const ControlStyle &overrides) {
  ControlStyle result;
  switch (role) {
  case ControlLayout::Button:
  case ControlLayout::Choice:
  case ControlLayout::Checkbox:
  case ControlLayout::Switch:
  case ControlLayout::Trigger:
    result.padding = math::Insets::all(
        role == ControlLayout::Button ? m.buttonPadding : m.padding);
    result.minimumHeight = m.controlHeight;
    if (role == ControlLayout::Checkbox)
      result.padding->left += m.indicatorSize + m.indicatorGap;
    if (role == ControlLayout::Switch)
      result.padding->left += m.switchWidth + m.indicatorGap;
    if (role == ControlLayout::Trigger)
      result.padding->right += m.chevronWidth + m.indicatorGap;
    break;
  case ControlLayout::StepperButton:
    result.minimumHeight = m.stepper.minimumHeight;
    result.width = m.stepper.buttonWidth;
    break;
  case ControlLayout::StepperCenter:
    result.minimumHeight = m.stepper.minimumHeight;
    break;
  case ControlLayout::StepperRow:
    result.gap = m.stepper.gap;
    break;
  case ControlLayout::FieldStack:
    result.gap = m.fieldGap;
    break;
  case ControlLayout::FieldRow:
    result.gap = m.sectionGap;
    break;
  case ControlLayout::Group:
    result.gap = m.gap;
    break;
  case ControlLayout::Settings:
    result.padding = math::Insets::all(m.settingsPadding);
    break;
  case ControlLayout::InputGroup:
    result.width = m.inputWidth;
    break;
  case ControlLayout::Section:
    result.gap = m.sectionGap;
    break;
  default:
    break;
  }
  if (overrides.padding)
    result.padding = overrides.padding;
  if (overrides.minimumHeight)
    result.minimumHeight = overrides.minimumHeight;
  if (overrides.width)
    result.width = overrides.width;
  if (overrides.gap)
    result.gap = overrides.gap;
  return result;
}

} // namespace playground::ui
