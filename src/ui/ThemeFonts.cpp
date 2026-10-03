#include <platform/sdl/SDLGeometry.hpp>
#include <support/AssetRegistry.hpp>
#include <ui/Theme.hpp>

namespace playground::ui {
FontHandle
resolveThemeFont(const ThemeTypography &typography,
                 std::optional<TextRole> role, FontHandle fallback,
                 AssetRegistry *assets, std::optional<FontFamily> family,
                 std::optional<FontSelection> selection,
                 std::shared_ptr<runtime::ResourceLedger> resources) {
  if (family &&
      (*family < FontFamily::Interface || *family >= FontFamily::Count))
    throw std::invalid_argument("Invalid font family");
  if (selection)
    selection->validate();
  FontProps props =
      fallback ? fallback->props() : FontProps{.style = {.size = 18}};
  float size = props.style.size;
  std::optional<float> lineHeight;
  if (role) {
    if (*role < TextRole::Display || *role >= TextRole::Count)
      throw std::invalid_argument("Invalid text role");
    const auto &style = typography.styles[static_cast<std::size_t>(*role)];
    if (!family)
      family = style.family;
    if (!selection)
      selection = style.face;
    size = style.size;
    lineHeight = style.lineHeight;
  }
  bool selected{};
  if (family) {
    if (const auto &definition =
            typography.families[static_cast<std::size_t>(*family)]) {
      const auto &face =
          selectFontFace(*definition, selection.value_or(FontSelection{}));
      props.path = face.path;
      props.cacheIdentity = face.cacheIdentity;
      props.style.flags &= ~(TTF_STYLE_BOLD | TTF_STYLE_ITALIC);
      selected = true;
    }
  }
  if (!selected && !fallback)
    throw std::invalid_argument(
        "Text requires a theme family or fallback font");
  if (!selected && selection) {
    props.style.flags &= ~(TTF_STYLE_BOLD | TTF_STYLE_ITALIC);
    if (selection->weight >= 600)
      props.style.flags |= TTF_STYLE_BOLD;
    if (selection->slant == FontSlant::Italic)
      props.style.flags |= TTF_STYLE_ITALIC;
  }
  if (typography.fallbacks) {
    props.fallbacks.clear();
    for (const auto &definition : *typography.fallbacks) {
      if (!definition)
        throw std::invalid_argument("Null fallback font family");
      const auto &face =
          selectFontFace(*definition, selection.value_or(FontSelection{}));
      props.fallbacks.push_back({face.path, face.cacheIdentity});
    }
  }
  if (!role && !family && !selection && typography.textScale == 1 && fallback &&
      props.fallbacks == fallback->props().fallbacks)
    return fallback;
  props.style.size = size * typography.textScale;
  if (lineHeight)
    props.layout.lineSpace = sdl::checkedPixel(
        std::ceil(static_cast<double>(props.style.size) * *lineHeight));
  else if (props.layout.lineSpace)
    props.layout.lineSpace = sdl::checkedPixel(std::ceil(
        static_cast<double>(*props.layout.lineSpace) * typography.textScale));
  if (assets)
    return assets->getFont(std::move(props));
  if (fallback)
    resources = fallback->resources();
  if (!resources)
    resources = runtime::defaultResourceLedger();
  return std::make_shared<Font>(std::move(props), std::move(resources));
}
} // namespace playground::ui
