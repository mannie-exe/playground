#include <platform/sdl/SDLGeometry.hpp>
#include <support/AssetRegistry.hpp>
#include <ui/Theme.hpp>

namespace playground::ui {
FontHandle resolveThemeFont(const ThemeTypography &typography,
                            std::optional<TextRole> role, FontHandle fallback,
                            AssetRegistry *assets) {
  float size{}, lineHeight{};
  bool emphasized{};
  if (role) {
    if (*role < TextRole::Display || *role >= TextRole::Count)
      throw std::invalid_argument("Invalid text role");
    const auto &style = typography.styles[static_cast<std::size_t>(*role)];
    if (auto family =
            typography.families[static_cast<std::size_t>(style.family)])
      fallback = std::move(family);
    size = style.size;
    lineHeight = style.lineHeight;
    emphasized = style.emphasized;
  }
  if (!fallback)
    throw std::invalid_argument(
        "Text requires a theme family or fallback font");
  if (!role && typography.textScale == 1)
    return fallback;
  if (!role)
    size = fallback->getSize();
  size *= typography.textScale;
  auto flags = fallback->props().style.flags;
  if (emphasized)
    flags |= TTF_STYLE_BOLD;
  const auto lineSpace =
      role ? std::optional<int>{sdl::checkedPixel(
                 std::ceil(static_cast<double>(size) * lineHeight))}
      : fallback->props().layout.lineSpace
          ? std::optional<int>{sdl::checkedPixel(std::ceil(
                static_cast<double>(*fallback->props().layout.lineSpace) *
                typography.textScale))}
          : std::nullopt;
  if (assets) {
    auto props = fallback->props();
    props.style.size = size;
    props.style.flags = flags;
    props.layout.lineSpace = lineSpace;
    return assets->getFont(std::move(props));
  }
  return std::make_shared<Font>(fallback->cloneWith(
      {.size = size, .flags = flags, .lineSpace = lineSpace}));
}
} // namespace playground::ui
