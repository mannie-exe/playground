#include <support/Font.hpp>

FontInfo Font::getInfo(const FontResource &font) {
  TTF_Font *sdlFont = font.get();

  return FontInfo{TTF_GetFontWeight(sdlFont), TTF_GetFontAscent(sdlFont),
                  TTF_GetFontDescent(sdlFont), TTF_FontIsFixedWidth(sdlFont),
                  TTF_FontIsScalable(sdlFont)};
}

void Font::setSize(float size) {
  if (!TTF_SetFontSize(_font.get(), size)) {
    throwSDLError(std::format(
        "Font@{} Failed to update TTF_Font@{} size from {} to {}", (void *)this,
        (void *)_font.get(), _props.style.size, size));
  }

  _props.style.size = size;
}

void Font::setOutline(int outline) {
  if (!TTF_SetFontOutline(_font.get(), outline)) {
    throwSDLError(std::format(
        "Font@{} Failed to update TTF_Font@{} outline from {} to {}",
        (void *)this, (void *)_font.get(), _props.style.outline, outline));
  }

  _props.style.outline = outline;
}

void Font::setDirection(TTF_Direction direction) {
  if (!TTF_SetFontDirection(_font.get(), direction)) {
    throwSDLError(std::format(
        "Font@{} Failed to update TTF_Font@{} direction from {} to {}",
        (void *)this, (void *)_font.get(), (int)_props.layout.direction,
        (int)direction));
  }

  _props.layout.direction = direction;
}

void Font::setSDF(bool sdf) {
  if (!TTF_SetFontSDF(_font.get(), sdf)) {
    throwSDLError(
        std::format("Font@{} Failed to update TTF_Font@{} sdf from {} to {}",
                    (void *)this, (void *)_font.get(), _props.render.sdf, sdf));
  }

  _props.render.sdf = sdf;
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
      .render = {
          .hinting = patch.hinting ? *patch.hinting : _props.render.hinting,
          .sdf = patch.sdf ? *patch.sdf : _props.render.sdf,
          .kern = patch.kern ? *patch.kern : _props.render.kern,
      }};
  return Font{cloneProps};
}

void Font::configureFont(const FontProps &props) {
  setSize(props.style.size);
  setStyleFlags(props.style.flags);
  setOutline(props.style.outline);

  setAlignment(props.layout.alignment);
  setDirection(props.layout.direction);
  setLineSpace(props.layout.lineSpace);

  setHinting(props.render.hinting);
  setSDF(props.render.sdf);
  setKerning(props.render.kern);
}

Font::Font(FontProps props)
    : _font{requireSDL(
          TTF_OpenFont(props.path.c_str(), props.style.size),
          std::format("Font@{} Failed to load {}", (void *)this, props.path))},
      _props{std::move(props)} {
  configureFont(_props);
}
