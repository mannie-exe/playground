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
  if (high)
    return {rgb(dark ? 0x000000 : 0xffffff), rgb(dark ? 0x000000 : 0xffffff),
            rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffffff : 0x000000),
            rgb(dark ? 0xffffff : 0x000000), rgb(dark ? 0xffff00 : 0x000080),
            rgb(dark ? 0x000000 : 0xffffff), rgb(dark ? 0x303030 : 0xe0e0e0),
            rgb(dark ? 0x505050 : 0xc0c0c0), rgb(dark ? 0x00ffff : 0x0000aa),
            rgb(dark ? 0x404000 : 0xd0d0ff), true};
  if (dark)
    return {rgb(0x181c22), rgb(0x252c36), rgb(0xf0f3f7), rgb(0xaeb9c8),
            rgb(0x748297), rgb(0x90bfff), rgb(0x102340), rgb(0x344153),
            rgb(0x435570), rgb(0xffdb80), rgb(0x344d70), false};
  return {rgb(0xf2f4f7), rgb(0xffffff), rgb(0x17212e), rgb(0x506074),
          rgb(0x687a90), rgb(0x195bb5), rgb(0xffffff), rgb(0xe0e9f5),
          rgb(0xcbdaf0), rgb(0x754400), rgb(0xd4e4fa), false};
}

const ThemePalette &defaultTheme() {
  static const auto value = resolveTheme(ColorSchemePreference::Light,
                                         ContrastPreference::Normal, {});
  return value;
}
} // namespace playground::ui
