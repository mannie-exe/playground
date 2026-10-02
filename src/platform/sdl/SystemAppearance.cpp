#include <SDL3/SDL.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <platform/sdl/SystemAppearance.hpp>

namespace playground::sdl {
#if defined(__APPLE__) || defined(__linux__)
void platformContrast(ui::SystemAppearance &);
#endif
ui::SystemAppearance systemAppearance() {
  static ui::SystemAppearance cached;
  static Uint64 nextPoll{};
  const auto now = SDL_GetTicks();
  if (now < nextPoll)
    return cached;
  nextPoll = now + 250;
  ui::SystemAppearance value;
  const auto scheme = SDL_GetSystemTheme();
  if (scheme != SDL_SYSTEM_THEME_UNKNOWN)
    value.dark = scheme == SDL_SYSTEM_THEME_DARK;
#if defined(_WIN32)
  HIGHCONTRASTW contrast{sizeof(HIGHCONTRASTW)};
  if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast,
                            0)) {
    value.highContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    if (*value.highContrast) {
      auto colors = ui::resolveTheme(ui::ColorSchemePreference::System,
                                     ui::ContrastPreference::High, {});
      const auto color = [](int index) {
        auto c = GetSysColor(index);
        return math::ColorRGBA8{GetRValue(c), GetGValue(c), GetBValue(c), 255};
      };
      colors.surface = colors.elevated = color(COLOR_WINDOW);
      colors.text = color(COLOR_WINDOWTEXT);
      colors.mutedText = color(COLOR_GRAYTEXT);
      colors.border = color(COLOR_WINDOWTEXT);
      colors.accent = colors.focus = color(COLOR_HIGHLIGHT);
      colors.onAccent = color(COLOR_HIGHLIGHTTEXT);
      colors.selection = colors.surface;
      colors.scrollbar = colors.border;
      colors.error = colors.warning = colors.success = colors.text;
      colors.hover = colors.pressed = colors.surface;
      value.contrastPalette = colors;
    }
  }
#elif defined(__APPLE__) || defined(__linux__)
  platformContrast(value);
#endif
  cached = value;
  return cached;
}
} // namespace playground::sdl
