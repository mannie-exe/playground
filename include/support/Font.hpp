#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3_ttf/SDL_ttf.h>

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
  // Baseline advance in pixels; absent uses the font's natural line skip.
  std::optional<int> lineSpace;
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

struct FontPatch {
  std::optional<std::string> path;

  std::optional<float> size;
  std::optional<TTF_FontStyleFlags> flags;
  std::optional<int> outline;

  std::optional<TTF_HorizontalAlignment> alignment;
  std::optional<TTF_Direction> direction;
  std::optional<std::optional<int>> lineSpace;

  std::optional<TTF_HintingFlags> hinting;
  std::optional<bool> sdf;
  std::optional<bool> kern;
};

class Font {
  FontResource _font;
  FontProps _props;
  int _naturalLineSkip{};

  static FontInfo getInfo(const FontResource &font);

public:
  explicit Font(FontProps props);

  TTF_Font *get() const { return _font.get(); }
  const FontProps &getProps() const noexcept { return _props; }

  std::string_view getPath() const { return _props.path; }

  float getSize() const { return _props.style.size; }
  TTF_FontStyleFlags getStyleFlags() const { return _props.style.flags; }
  int getOutline() const { return _props.style.outline; }

  TTF_HorizontalAlignment getAlignment() const {
    return _props.layout.alignment;
  }
  TTF_Direction getDirection() const { return _props.layout.direction; }
  std::optional<int> getLineSpace() const { return _props.layout.lineSpace; }
  int getLineSkip() const { return TTF_GetFontLineSkip(_font.get()); }

  TTF_HintingFlags getHinting() const { return _props.render.hinting; }
  bool isSDF() const { return _props.render.sdf; }
  bool isKerningEnabled() const { return _props.render.kern; }

  FontInfo getInfo() const { return getInfo(_font); }

  void setPath(const std::string &path) {
    Font next = cloneWith({.path = path});
    *this = std::move(next);
  }

  void setSize(float size);

  void setStyleFlags(TTF_FontStyleFlags format);

  void setOutline(int outline);

  void setAlignment(TTF_HorizontalAlignment alignment) {
    TTF_SetFontWrapAlignment(_font.get(), alignment);
    _props.layout.alignment = alignment;
  }

  void setDirection(TTF_Direction direction);

  void setLineSpace(std::optional<int> lineSpace);

  void setHinting(TTF_HintingFlags hinting) {
    TTF_SetFontHinting(_font.get(), hinting);
    _props.render.hinting = hinting;
  }

  void setSDF(bool sdf);

  void setKerning(bool kern) {
    TTF_SetFontKerning(_font.get(), kern);
    _props.render.kern = kern;
  }

  void applyProps(FontPatch patch);

  Font cloneWith(FontPatch patch) const;

  Font(Font &&) noexcept = default;
  Font &operator=(Font &&) noexcept = default;

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

private:
  void configureFont(const FontProps &props);
};

using FontHandle = std::shared_ptr<const Font>;
