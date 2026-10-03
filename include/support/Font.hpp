#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

#include <rendering/ResourceLedger.hpp>
#include <support/FontHandle.hpp>
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

struct FontFallback {
  std::string path, cacheIdentity;
  bool operator==(const FontFallback &) const = default;
};

struct FontProps {
  std::string path;
  // Optional catalog-generation identity; copied by font variants.
  std::string cacheIdentity;

  FontStyleProps style;
  FontLayoutProps layout;
  FontRenderProps render;
  std::vector<FontFallback> fallbacks;
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
  // Close the primary face before its borrowed SDL_ttf fallback faces.
  std::vector<FontHandle> _fallbacks;
  FontResource _font;
  FontProps _props;
  std::shared_ptr<playground::rendering::ResourceLedger> _ledger;
  int _naturalLineSkip{};

  static FontInfo getInfo(const FontResource &font);
  friend class AssetRegistry;
  Font(FontProps props, std::vector<FontHandle> fallbacks,
       std::shared_ptr<playground::rendering::ResourceLedger> ledger);

public:
  explicit Font(FontProps props,
                std::shared_ptr<playground::rendering::ResourceLedger> ledger =
                    playground::rendering::defaultResourceLedger());

  const auto &resources() const noexcept { return _ledger; }

  TTF_Font *get() const { return _font.get(); }

  const FontProps &props() const noexcept { return _props; }

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

  void setHinting(TTF_HintingFlags hinting);

  void setSDF(bool sdf);

  void setKerning(bool kern);

  void applyProps(FontPatch patch);

  Font cloneWith(FontPatch patch) const;

  Font(Font &&) noexcept = default;

  Font &operator=(Font &&other) noexcept {
    // Keep each primary face paired with its fallback owners until teardown.
    using std::swap;
    swap(_fallbacks, other._fallbacks);
    swap(_font, other._font);
    swap(_props, other._props);
    swap(_ledger, other._ledger);
    swap(_naturalLineSkip, other._naturalLineSkip);
    return *this;
  }

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

private:
  void configureFont(const FontProps &props);
};
