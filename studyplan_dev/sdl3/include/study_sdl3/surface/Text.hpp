#pragma once

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL_rect.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>
#include <study_sdl3/surface/DrawableSurface.hpp>
#include <study_sdl3/surface/Font.hpp>

using TextResource = SDLResource<TTF_Text, TTF_DestroyText>;

enum class TextRenderMethod {
  Blended,
  Solid,
  Shaded,
  LCD,
};

constexpr std::string_view toString(TextRenderMethod method) {
  switch (method) {
  case TextRenderMethod::Blended:
    return "Blended";
  case TextRenderMethod::Shaded:
    return "Shaded";
  case TextRenderMethod::Solid:
    return "Solid";
  case TextRenderMethod::LCD:
    return "LCD";
  default:
    return "Unknown";
  }
}

template <>
struct std::formatter<TextRenderMethod> : std::formatter<std::string_view> {
  auto format(TextRenderMethod method, format_context &ctx) const {
    return std::formatter<std::string_view>::format(toString(method), ctx);
  }
};

struct TextStyleProps {
  TextRenderMethod renderMethod{TextRenderMethod::Blended};
  SDL_Color fgColor{255, 255, 255, 255};
  SDL_Color bgColor{0, 0, 0, 255};
};

struct TextLayoutProps {
  int scaleWidth{0};
  int wrapWidth{0};
};

struct TextProps {
  std::string value;

  TextStyleProps style;
  TextLayoutProps layout;
};

class Text : public DrawableSurface {
  struct TextRenderState {
    Font font;
    TextResource text;

    TextRenderState(Font font, TextResource text)
        : font{std::move(font)}, text{std::move(text)} {}

    TextRenderState(TextRenderState &&) noexcept = default;
    TextRenderState &operator=(TextRenderState &&) noexcept = default;

    TextRenderState(const TextRenderState &) = delete;
    TextRenderState &operator=(const TextRenderState &) = delete;
  };

  TextProps _props;
  TextRenderState _baseState;
  std::optional<TextRenderState> _scaledState;

  static SurfaceResource createSurface(const void *owner,
                                       const TextProps &props,
                                       const TextRenderState &state) {
    if (props.layout.wrapWidth > 0) {
      switch (props.style.renderMethod) {
      case TextRenderMethod::Blended:
        return SurfaceResource{requireSDL(
            TTF_RenderText_Blended_Wrapped(
                state.font.get(), props.value.c_str(), 0, props.style.fgColor,
                props.layout.wrapWidth),
            std::format("Text@{} Failed to create SDL_Surface", owner))};
      case TextRenderMethod::Shaded:
        return SurfaceResource{requireSDL(
            TTF_RenderText_Shaded_Wrapped(
                state.font.get(), props.value.c_str(), 0, props.style.fgColor,
                props.style.bgColor, props.layout.wrapWidth),
            std::format("Text@{} Failed to create SDL_Surface", owner))};
      case TextRenderMethod::Solid:
        return SurfaceResource{requireSDL(
            TTF_RenderText_Solid_Wrapped(state.font.get(), props.value.c_str(),
                                         0, props.style.fgColor,
                                         props.layout.wrapWidth),
            std::format("Text@{} Failed to create SDL_Surface", owner))};
      case TextRenderMethod::LCD:
        return SurfaceResource{requireSDL(
            TTF_RenderText_LCD_Wrapped(state.font.get(), props.value.c_str(), 0,
                                       props.style.fgColor, props.style.bgColor,
                                       props.layout.wrapWidth),
            std::format("Text@{} Failed to create SDL_Surface", owner))};
      };
    }

    switch (props.style.renderMethod) {
    case TextRenderMethod::Blended:
      return SurfaceResource{requireSDL(
          TTF_RenderText_Blended(state.font.get(), props.value.c_str(), 0,
                                 props.style.fgColor),
          std::format("Text@{} Failed to create SDL_Surface", owner))};
    case TextRenderMethod::Shaded:
      return SurfaceResource{requireSDL(
          TTF_RenderText_Shaded(state.font.get(), props.value.c_str(), 0,
                                props.style.fgColor, props.style.bgColor),
          std::format("Text@{} Failed to create SDL_Surface", owner))};
    case TextRenderMethod::Solid:
      return SurfaceResource{requireSDL(
          TTF_RenderText_Solid(state.font.get(), props.value.c_str(), 0,
                               props.style.fgColor),
          std::format("Text@{} Failed to create SDL_Surface", owner))};
    case TextRenderMethod::LCD:
      return SurfaceResource{requireSDL(
          TTF_RenderText_LCD(state.font.get(), props.value.c_str(), 0,
                             props.style.fgColor, props.style.bgColor),
          std::format("Text@{} Failed to create SDL_Surface", owner))};
    };

    throwSDLError(
        std::format("Text@{} received unknown TextRenderMethod", owner));
  }

  static TextResource createText(const void *owner, const std::string &value,
                                 const Font &font) {
    return TextResource{
        requireSDL(TTF_CreateText(nullptr, font.get(), value.c_str(), 0),
                   std::format("Text@{} Failed to create TTF_Text", owner))};
  }

  static TextRenderState createRenderState(const void *owner,
                                           const std::string &value,
                                           Font font) {
    TextResource text{createText(owner, value, font)};
    return TextRenderState{std::move(font), std::move(text)};
  }

  static SDL_Point getSizeFromText(const void *owner,
                                   const TextResource &text) {
    SDL_Point size;
    if (!TTF_GetTextSize(text.get(), &size.x, &size.y))
      throwSDLError(std::format("Text@{} Failed to update size", owner));
    return size;
  }

  static std::optional<TextRenderState>
  createScaledState(const void *owner, const TextProps &props,
                    const TextRenderState &baseState) {
    std::optional<TextRenderState> scaledState;

    SurfaceResource wrappedSurface{createSurface(owner, props, baseState)};

    if (props.layout.scaleWidth > 0 && wrappedSurface->w > 0) {
      float targetSize =
          baseState.font.getSize() * props.layout.scaleWidth / wrappedSurface->w;
      scaledState = createRenderState(
          owner, props.value, baseState.font.cloneWith({.size = targetSize}));
    }

    return scaledState;
  }

public:
  Text(TextProps props, Font font, bool autoConvert = false)
      : DrawableSurface{autoConvert}, _props{std::move(props)},
        _baseState{createRenderState(this, _props.value, std::move(font))},
        _scaledState{createScaledState(this, _props, _baseState)} {
    replaceSurface(createCurrentSurface());
  }

  const std::string &getValue() const { return _props.value; }
  TextRenderMethod getRenderMethod() const { return _props.style.renderMethod; }
  SDL_Color getFGColor() const { return _props.style.fgColor; }
  SDL_Color getBGColor() const { return _props.style.bgColor; }
  int getScaleWidth() const { return _props.layout.scaleWidth; }
  int getWrapWidth() const { return _props.layout.wrapWidth; }

  void setValue(const std::string &value) {
    if (value.empty() || value == _props.value)
      return;

    try {
      TextProps props{
          .value = value,
          .style = _props.style,
          .layout = _props.layout,
      };
      TextRenderState cache{
          createRenderState(this, props.value, _baseState.font.cloneWith({}))};
      std::optional<TextRenderState> scaledState =
          createScaledState(this, props, cache);
      SurfaceResource surface{
          createSurface(this, props, scaledState ? *scaledState : cache)};

      _props = std::move(props);
      _scaledState = std::move(scaledState);
      _baseState = std::move(cache);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log(
          "%s",
          buildSDLErrorMessage(
              std::format("Text@{} Failed to change value from {} to: {}\n  {}",
                          (void *)this, _props.value, value, err.c_str()))
              .c_str());
    }
  }

  void setRenderMethod(const TextRenderMethod renderMethod) {
    try {
      TextProps props = _props;
      props.style.renderMethod = renderMethod;
      std::optional<TextRenderState> scaledState =
          createScaledState(this, props, _baseState);
      SurfaceResource surface{
          createSurface(this, props, scaledState ? *scaledState : _baseState)};

      _props = std::move(props);
      _scaledState = std::move(scaledState);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log("%s",
              buildSDLErrorMessage(
                  std::format(
                      "Text@{} Failed to change renderMethod from {} to: {}",
                      (void *)this, _props.style.renderMethod, renderMethod))
                  .c_str());
    }
  }

  void setFGColor(const SDL_Color color) {
    try {
      TextProps props = _props;
      props.style.fgColor = color;
      SurfaceResource surface{
          createSurface(this, props, _scaledState ? *_scaledState : _baseState)};

      _props = std::move(props);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      auto [_r, _g, _b, _a] = _props.style.fgColor;
      auto [r, g, b, a] = color;
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format("Text@{} Failed to change fgColor from "
                                    "{{{},{},{},{}}} to: {{{},{},{},{}}}\n  {}",
                                    (void *)this, _r, _g, _b, _a, r, g, b, a,
                                    err.c_str()))
                        .c_str());
    }
  }

  void setBGColor(const SDL_Color color) {
    try {
      TextProps props = _props;
      props.style.bgColor = color;
      SurfaceResource surface{
          createSurface(this, props, _scaledState ? *_scaledState : _baseState)};

      _props = std::move(props);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      auto [_r, _g, _b, _a] = _props.style.bgColor;
      auto [r, g, b, a] = color;
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format("Text@{} Failed to change bgColor from "
                                    "{{{},{},{},{}}} to: {{{},{},{},{}}}\n  {}",
                                    (void *)this, _r, _g, _b, _a, r, g, b, a,
                                    err.c_str()))
                        .c_str());
    }
  }

  void setWrapWidth(const int wrapWidth) {
    if (_props.layout.wrapWidth == wrapWidth)
      return;

    const int finalWrapWidth = wrapWidth > 0 ? wrapWidth : 0;

    try {
      TextProps props = _props;
      props.layout.wrapWidth = finalWrapWidth;
      std::optional<TextRenderState> scaledState =
          createScaledState(this, props, _baseState);
      SurfaceResource surface{
          createSurface(this, props, scaledState ? *scaledState : _baseState)};

      _props = std::move(props);
      _scaledState = std::move(scaledState);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format(
                            "Text@{} Failed to change wrapWidth from {} to: {}",
                            (void *)this, _props.layout.wrapWidth, wrapWidth))
                        .c_str());
    }
  }

  void setScaleWidth(const int scaleWidth) {
    if (_props.layout.scaleWidth == scaleWidth)
      return;

    try {
      TextProps props = _props;
      props.layout.scaleWidth = scaleWidth;
      std::optional<TextRenderState> scaledState =
          createScaledState(this, props, _baseState);
      SurfaceResource surface{
          createSurface(this, props, scaledState ? *scaledState : _baseState)};

      _props = std::move(props);
      _scaledState = std::move(scaledState);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log(
          "%s",
          buildSDLErrorMessage(
              std::format("Text@{} Failed to change scaleWidth from {} to: {}",
                          (void *)this, _props.layout.scaleWidth, scaleWidth))
              .c_str());
    }
  }

  void setFontSize(float size) {
    try {
      replaceFont(_baseState.font.cloneWith({.size = size}));
    } catch (const std::string &err) {
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format("Text@{} Failed to set font size to: {}",
                                    (void *)this, size))
                        .c_str());
    }
  }

  void replaceFont(Font font) {
    try {
      TextRenderState cache{
          createRenderState(this, _props.value, std::move(font))};
      std::optional<TextRenderState> scaledState =
          createScaledState(this, _props, cache);
      SurfaceResource surface{
          createSurface(this, _props, scaledState ? *scaledState : cache)};

      _baseState = std::move(cache);
      _scaledState = std::move(scaledState);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log("%s",
              std::format("Text@{} Failed to replace font with: Font@{}\n  {}",
                          (void *)this, (void *)&font, err.c_str())
                  .c_str());
    }
  }

  SurfaceResource createCurrentSurface() const {
    return createSurface(this, _props, _scaledState ? *_scaledState : _baseState);
  }

  Text(Text &&) noexcept = default;
  Text &operator=(Text &&) noexcept = default;

  Text(const Text &) = delete;
  Text &operator=(const Text &) = delete;
};
