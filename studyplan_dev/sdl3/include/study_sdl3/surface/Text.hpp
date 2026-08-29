#pragma once

#include <format>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_rect.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>
#include <study_sdl3/surface/DrawableSurface.hpp>
#include <study_sdl3/surface/Font.hpp>

class Text : public DrawableSurface {
  struct ScaledText {
    Font font;
    SDLResource<TTF_Text, TTF_DestroyText> text;
    SDL_Point size{};
  };

  std::string _value;
  Font _font;
  SDL_Color _color;
  int _targetWidth;

  SDLResource<TTF_Text, TTF_DestroyText> _text;
  SDL_Point _size;

  std::optional<ScaledText> _scaled;

public:
  Text(const std::string &value, Font font,
       const SDL_Color color = SDL_Color{0, 255, 255, 255},
       const int targetWidth = 0, const bool autoConvert = false)
      : DrawableSurface(createSurface(this, value, font, color), autoConvert),
        _value{value}, _font{std::move(font)}, _color{color},
        _targetWidth{targetWidth}, _text{createText(this, _value, _font)},
        _size{getSizeFromText(this, _text)},
        _scaled{scaleToWidth(this, _targetWidth, _size, _value, _font)} {
    if (_scaled)
      replaceSurface(createCurrentSurface());
  }

  const std::string &getValue() const { return _value; }
  SDL_Color getColor() const { return _color; }
  int getTargetWidth() const { return _targetWidth; }
  SDL_Point getSize() const { return _size; }

  void setValue(const std::string &value) {
    if (value.empty() || value == _value)
      return;

    try {
      SDLResource<TTF_Text, TTF_DestroyText> text{
          createText(this, value, _font)};
      SDL_Point size = getSizeFromText(this, text);
      std::optional<ScaledText> scaled =
          scaleToWidth(this, _targetWidth, size, value, _font);
      SDLResource<SDL_Surface, SDL_DestroySurface> surface{
          createSurface(this, value, scaled ? scaled->font : _font, _color)};

      _scaled = std::move(scaled);
      _size = size;
      _text.reset(text.release());
      _value = value;
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log(
          "%s",
          buildSDLErrorMessage(
              std::format("Text@{} Failed to change value from {} to: {}\n  {}",
                          (void *)this, _value, value, err.c_str()))
              .c_str());
    }
  }

  void setColor(const SDL_Color color) {
    try {
      SDLResource<SDL_Surface, SDL_DestroySurface> surface{
          createSurface(this, _value, _scaled ? _scaled->font : _font, color)};

      _color = color;
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      auto [_r, _g, _b, _a] = _color;
      auto [r, g, b, a] = color;
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format("Text@{} Failed to change color from "
                                    "{{{},{},{},{}}} to: {{{},{},{},{}}}\n  {}",
                                    (void *)this, _r, _g, _b, _a, r, g, b, a,
                                    err.c_str()))
                        .c_str());
    }
  }

  void setTargetWidth(const int targetWidth) {
    if (_targetWidth == targetWidth)
      return;

    try {
      std::optional<ScaledText> scaled =
          scaleToWidth(this, targetWidth, _size, _value, _font);
      SDLResource<SDL_Surface, SDL_DestroySurface> surface{
          createSurface(this, _value, scaled ? scaled->font : _font, _color)};

      _targetWidth = targetWidth;
      _scaled = std::move(scaled);
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log("%s", buildSDLErrorMessage(
                        std::format("Text@{} Failed set targetWidth to: {}",
                                    (void *)this, targetWidth))
                        .c_str());
    }
  }

  void replaceFont(Font font) {
    try {
      SDLResource<TTF_Text, TTF_DestroyText> text{
          createText(this, _value, font)};
      SDL_Point size = getSizeFromText(this, text);
      std::optional<ScaledText> scaled =
          scaleToWidth(this, _targetWidth, size, _value, font);
      SDLResource<SDL_Surface, SDL_DestroySurface> surface{
          createSurface(this, _value, scaled ? scaled->font : font, _color)};

      _scaled = std::move(scaled);
      _size = size;
      _font = std::move(font);
      _text.reset(text.release());
      replaceSurface(std::move(surface));
    } catch (const std::string &err) {
      SDL_Log("%s",
              std::format("Text@{} Failed to replace font with: Font@{}\n  {}",
                          (void *)this, (void *)&font, err.c_str())
                  .c_str());
    }
  }

  SDLResource<SDL_Surface, SDL_DestroySurface> createCurrentSurface() const {
    return createSurface(this, _value, _scaled ? _scaled->font : _font, _color);
  }

  Text(Text &&) noexcept = default;
  Text &operator=(Text &&) noexcept = default;

  Text(const Text &) = delete;
  Text &operator=(const Text &) = delete;

private:
  static SDLResource<SDL_Surface, SDL_DestroySurface>
  createSurface(const void *owner, const std::string &value, const Font &font,
                const SDL_Color &color) {
    return SDLResource<SDL_Surface, SDL_DestroySurface>{
        requireSDL(TTF_RenderText_Blended(font.get(), value.c_str(), 0, color),
                   std::format("Text@{} Failed to create SDL_Surface", owner))};
  }

  static SDLResource<TTF_Text, TTF_DestroyText>
  createText(const void *owner, const std::string &value, const Font &font) {
    return SDLResource<TTF_Text, TTF_DestroyText>{
        requireSDL(TTF_CreateText(nullptr, font.get(), value.c_str(), 0),
                   std::format("Text@{} Failed to create TTF_Text", owner))};
  }

  static SDL_Point
  getSizeFromText(const void *owner,
                  const SDLResource<TTF_Text, TTF_DestroyText> &text) {
    SDL_Point size;
    if (!TTF_GetTextSize(text.get(), &size.x, &size.y))
      throwSDLError(std::format("Text@{} Failed to update size", owner));
    return size;
  }

  static std::optional<ScaledText>
  scaleToWidth(const void *owner, const int targetWidth, const SDL_Point size,
               const std::string &value, const Font &font) {
    std::optional<ScaledText> scaled;

    if (targetWidth > 0 && size.x > 0) {
      float targetSize = font.getSize() * targetWidth / size.x;
      scaled = std::make_optional<ScaledText>(font.cloneWithSize(targetSize));
      scaled->text = createText(&scaled, value, scaled->font);
      scaled->size = getSizeFromText(owner, scaled->text);
    }

    return scaled;
  }

  void updateSizeFromText(const SDLResource<TTF_Text, TTF_DestroyText> &text) {
    if (!TTF_GetTextSize(text.get(), &_size.x, &_size.y))
      throwSDLError(std::format("Text@{} Failed to update size", (void *)this));
  }
};
