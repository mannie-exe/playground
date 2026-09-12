#pragma once

#include <format>
#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3_ttf/SDL_ttf.h>

#include <support/SDLError.hpp>
#include <support/SDLResource.hpp>

using FontResource = SDLResource<TTF_Font, TTF_CloseFont>;

struct FontInfo {
  int weight;
  int ascent, descent;
  bool monospaced;
  bool scalable;
};

struct FontStyleProps {
  float size{12.0f};
  TTF_FontStyleFlags flags{TTF_STYLE_NORMAL};
  int outline{0};
};

struct FontLayoutProps {
  TTF_HorizontalAlignment alignment{TTF_HORIZONTAL_ALIGN_LEFT};
  TTF_Direction direction{TTF_DIRECTION_LTR};
  int lineSpace{1};
};

struct FontRenderProps {
  TTF_HintingFlags hinting{TTF_HINTING_LIGHT};
  bool sdf{false};
  bool kern{true};
};

struct FontProps {
  std::string path;

  FontStyleProps style;
  FontLayoutProps layout;
  FontRenderProps render;
};

struct FontPropsPatch {
  std::optional<std::string> path;

  std::optional<float> size;
  std::optional<TTF_FontStyleFlags> flags;
  std::optional<int> outline;

  std::optional<TTF_HorizontalAlignment> alignment;
  std::optional<TTF_Direction> direction;
  std::optional<int> lineSpace;

  std::optional<TTF_HintingFlags> hinting;
  std::optional<bool> sdf;
  std::optional<bool> kern;
};

class Font {
  FontResource _font;
  FontProps _props;

  static FontInfo getInfo(const FontResource &font) {
    TTF_Font *sdlFont = font.get();

    return FontInfo{TTF_GetFontWeight(sdlFont), TTF_GetFontAscent(sdlFont),
                    TTF_GetFontDescent(sdlFont), TTF_FontIsFixedWidth(sdlFont),
                    TTF_FontIsScalable(sdlFont)};
  }

public:
  explicit Font(FontProps props)
      : _font{requireSDL(TTF_OpenFont(props.path.c_str(), props.style.size),
                         std::format("Font@{} Failed to load {}", (void *)this,
                                     props.path))},
        _props{std::move(props)} {
    configureFont(_props);
  };

  TTF_Font *get() const { return _font.get(); }

  std::string_view getPath() const { return _props.path; }

  float getSize() const { return _props.style.size; }
  TTF_FontStyleFlags getStyleFlags() const { return _props.style.flags; }
  int getOutline() const { return _props.style.outline; }

  TTF_HorizontalAlignment getAlignment() const {
    return _props.layout.alignment;
  }
  TTF_Direction getDirection() const { return _props.layout.direction; }
  int getLineSpace() const { return _props.layout.lineSpace; }

  TTF_HintingFlags getHinting() const { return _props.render.hinting; }
  bool isSDF() const { return _props.render.sdf; }
  bool isKerningEnabled() const { return _props.render.kern; }

  FontInfo getInfo() const { return getInfo(_font); }

  void setPath(const std::string &path) {
    Font next = cloneWith({.path = path});
    *this = std::move(next);
  }

  void setSize(float size) {
    if (!TTF_SetFontSize(_font.get(), size)) {
      throwSDLError(std::format(
          "Font@{} Failed to update TTF_Font@{} size from {} to {}",
          (void *)this, (void *)_font.get(), _props.style.size, size));
    }

    _props.style.size = size;
  }

  void setStyleFlags(TTF_FontStyleFlags format) {
    TTF_SetFontStyle(_font.get(), format);
    _props.style.flags = format;
  }

  void setOutline(int outline) {
    if (!TTF_SetFontOutline(_font.get(), outline)) {
      throwSDLError(std::format(
          "Font@{} Failed to update TTF_Font@{} outline from {} to {}",
          (void *)this, (void *)_font.get(), _props.style.outline, outline));
    }

    _props.style.outline = outline;
  }

  void setAlignment(TTF_HorizontalAlignment alignment) {
    TTF_SetFontWrapAlignment(_font.get(), alignment);
    _props.layout.alignment = alignment;
  }

  void setDirection(TTF_Direction direction) {
    if (!TTF_SetFontDirection(_font.get(), direction)) {
      throwSDLError(std::format(
          "Font@{} Failed to update TTF_Font@{} direction from {} to {}",
          (void *)this, (void *)_font.get(), (int)_props.layout.direction,
          (int)direction));
    }

    _props.layout.direction = direction;
  }

  void setLineSpace(int lineSpace) {
    TTF_SetFontLineSkip(_font.get(), lineSpace);
    _props.layout.lineSpace = lineSpace;
  }

  void setHinting(TTF_HintingFlags hinting) {
    TTF_SetFontHinting(_font.get(), hinting);
    _props.render.hinting = hinting;
  }

  void setSDF(bool sdf) {
    if (!TTF_SetFontSDF(_font.get(), sdf)) {
      throwSDLError(std::format(
          "Font@{} Failed to update TTF_Font@{} sdf from {} to {}",
          (void *)this, (void *)_font.get(), _props.render.sdf, sdf));
    }

    _props.render.sdf = sdf;
  }

  void setKerning(bool kern) {
    TTF_SetFontKerning(_font.get(), kern);
    _props.render.kern = kern;
  }

  void applyProps(FontPropsPatch patch) {
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

  Font cloneWith(FontPropsPatch patch) const {
    FontProps cloneProps{
        .path = patch.path ? *patch.path : _props.path,
        .style =
            {
                .size = patch.size ? *patch.size : _props.style.size,
                .flags = patch.flags ? *patch.flags : _props.style.flags,
                .outline =
                    patch.outline ? *patch.outline : _props.style.outline,
            },
        .layout =
            {
                .alignment = patch.alignment ? *patch.alignment
                                             : _props.layout.alignment,
                .direction = patch.direction ? *patch.direction
                                             : _props.layout.direction,
                .lineSpace = patch.lineSpace ? *patch.lineSpace
                                             : _props.layout.lineSpace,
            },
        .render = {
            .hinting = patch.hinting ? *patch.hinting : _props.render.hinting,
            .sdf = patch.sdf ? *patch.sdf : _props.render.sdf,
            .kern = patch.kern ? *patch.kern : _props.render.kern,
        }};
    return Font{cloneProps};
  }

  Font(Font &&) noexcept = default;
  Font &operator=(Font &&) noexcept = default;

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

private:
  void configureFont(const FontProps &props) {
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
};

using FontHandle = std::shared_ptr<const Font>;
