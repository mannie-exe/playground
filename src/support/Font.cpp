#include <format>
#include <stdexcept>
#include <utility>

#include <support/Font.hpp>
#include <support/SDLError.hpp>

FontInfo Font::getInfo(const FontResource &font) {
  TTF_Font *sdlFont = font.get();

  return FontInfo{TTF_GetFontWeight(sdlFont), TTF_GetFontAscent(sdlFont),
                  TTF_GetFontDescent(sdlFont), TTF_FontIsFixedWidth(sdlFont),
                  TTF_FontIsScalable(sdlFont)};
}

void Font::setSize(float size) {
  if (size != _props.style.size)
    *this = cloneWith({.size = size});
}

void Font::setStyleFlags(TTF_FontStyleFlags format) {
  if (format != _props.style.flags)
    *this = cloneWith({.flags = format});
}

void Font::setOutline(int outline) {
  if (outline != _props.style.outline)
    *this = cloneWith({.outline = outline});
}

void Font::setLineSpace(std::optional<int> lineSpace) {
  if (lineSpace && *lineSpace <= 0)
    throw std::invalid_argument("Font line spacing must be positive pixels");
  TTF_SetFontLineSkip(_font.get(), lineSpace.value_or(_naturalLineSkip));
  _props.layout.lineSpace = lineSpace;
}

void Font::setDirection(TTF_Direction direction) {
  if (direction != _props.layout.direction)
    *this = cloneWith({.direction = direction});
}

void Font::setSDF(bool sdf) {
  if (sdf != _props.render.sdf)
    *this = cloneWith({.sdf = sdf});
}

void Font::setHinting(TTF_HintingFlags hinting) {
  if (hinting != _props.render.hinting)
    *this = cloneWith({.hinting = hinting});
}

void Font::setKerning(bool kern) {
  if (kern != _props.render.kern)
    *this = cloneWith({.kern = kern});
}

void Font::applyProps(FontPatch patch) {
  if (patch.path && *patch.path != _props.path)
    setPath(*patch.path);

  if (patch.size && *patch.size != _props.style.size)
    setSize(*patch.size);
  if (patch.flags && *patch.flags != _props.style.flags)
    setStyleFlags(*patch.flags);
  if (patch.outline && *patch.outline != _props.style.outline)
    setOutline(*patch.outline);

  if (patch.alignment && *patch.alignment != _props.layout.alignment)
    setAlignment(*patch.alignment);
  if (patch.direction && *patch.direction != _props.layout.direction)
    setDirection(*patch.direction);
  if (patch.lineSpace && *patch.lineSpace != _props.layout.lineSpace)
    setLineSpace(*patch.lineSpace);

  if (patch.hinting && *patch.hinting != _props.render.hinting)
    setHinting(*patch.hinting);
  if (patch.sdf && *patch.sdf != _props.render.sdf)
    setSDF(*patch.sdf);
  if (patch.kern && *patch.kern != _props.render.kern)
    setKerning(*patch.kern);
}

Font Font::cloneWith(FontPatch patch) const {
  FontProps cloneProps{
      .path = patch.path ? *patch.path : _props.path,
      .cacheIdentity = patch.path ? std::string{} : _props.cacheIdentity,
      .style =
          {
              .size = patch.size ? *patch.size : _props.style.size,
              .flags = patch.flags ? *patch.flags : _props.style.flags,
              .outline = patch.outline ? *patch.outline : _props.style.outline,
          },
      .layout =
          {
              .alignment =
                  patch.alignment ? *patch.alignment : _props.layout.alignment,
              .direction =
                  patch.direction ? *patch.direction : _props.layout.direction,
              .lineSpace =
                  patch.lineSpace ? *patch.lineSpace : _props.layout.lineSpace,
          },
      .render =
          {
              .hinting = patch.hinting ? *patch.hinting : _props.render.hinting,
              .sdf = patch.sdf ? *patch.sdf : _props.render.sdf,
              .kern = patch.kern ? *patch.kern : _props.render.kern,
          },
      .fallbacks = _props.fallbacks};
  return Font{cloneProps};
}

void Font::configureFont(const FontProps &props) {
  TTF_SetFontStyle(_font.get(), props.style.flags);
  if (!TTF_SetFontOutline(_font.get(), props.style.outline))
    throwSDLError("Failed to configure font outline");

  setAlignment(props.layout.alignment);
  if (!TTF_SetFontDirection(_font.get(), props.layout.direction))
    throwSDLError("Failed to configure font direction");

  TTF_SetFontHinting(_font.get(), props.render.hinting);
  if (!TTF_SetFontSDF(_font.get(), props.render.sdf))
    throwSDLError("Failed to configure font SDF");
  TTF_SetFontKerning(_font.get(), props.render.kern);

  _naturalLineSkip = TTF_GetFontLineSkip(_font.get());
  setLineSpace(props.layout.lineSpace);
}

Font::Font(FontProps props) : Font{std::move(props), {}} {}

Font::Font(FontProps props, std::vector<FontHandle> fallbacks)
    : _fallbacks{std::move(fallbacks)},
      _font{requireSDL(
          TTF_OpenFont(props.path.c_str(), props.style.size),
          std::format("Font@{} Failed to load {}", (void *)this, props.path))},
      _props{std::move(props)} {
  configureFont(_props);
  if (_fallbacks.empty())
    for (const auto &source : _props.fallbacks) {
      if (source.path.empty() || source.path.find('\0') != std::string::npos)
        throw std::invalid_argument("Invalid fallback font path");
      auto props = _props;
      props.path = source.path;
      props.cacheIdentity = source.cacheIdentity;
      props.fallbacks.clear();
      _fallbacks.push_back(std::make_shared<const Font>(std::move(props)));
    }
  for (const auto &font : _fallbacks)
    if (!TTF_AddFallbackFont(_font.get(), font->get()))
      throwSDLError("Cannot attach fallback font");
}
