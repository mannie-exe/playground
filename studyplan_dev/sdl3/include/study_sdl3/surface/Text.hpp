#pragma once

#include <format>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <study_sdl3/surface/DrawableSurface.hpp>
#include <study_sdl3/surface/Font.hpp>

class Text : public DrawableSurface {
  TTF_Text *_text;
  Font *_font;
  std::string _value;
  SDL_Color _color;

public:
  Text(const std::string &value, Font &font,
       SDL_Color color = SDL_Color{0, 255, 255, 255}, bool autoConvert = false)
      : DrawableSurface(
            TTF_RenderText_Blended(font.get(), value.c_str(), 0, color),
            autoConvert),
        _value{value}, _font{&font}, _color{color},
        _text{TTF_CreateText(nullptr, font.get(), value.c_str(), 0)} {
    if (!_text) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format("Text@{} Failed to create: TTF_Text@{}\n  {}",
                          (void *)this, (void *)_text, err);
        SDL_ClearError();
        throw err;
      }

      throw std::format("Text@{} Failed to create: TTF_Text@{}", (void *)this,
                        (void *)_text);
    }
  }

  ~Text() {
    if (_text) {
      TTF_DestroyText(_text);
    }
  }

  const std::string &getValue() { return _value; }

  void setValue(const std::string &value) {
    if (value.empty())
      return;

    try {
      TTF_Text *text = TTF_CreateText(nullptr, _font->get(), value.c_str(), 0);
      if (!text) {
        std::string err{SDL_GetError()};
        if (!err.empty()) {
          err = std::format("Text@{} Failed to create: TTF_Text@{}\n  {}",
                            (void *)this, (void *)text, err);
          SDL_ClearError();
          throw err;
        }

        throw std::format("Text@{} Failed to create: TTF_Text@{}", (void *)this,
                          (void *)text);
      }
      TTF_DestroyText(_text);
      _value = value;
      _text = text;
      replaceSurface(
          TTF_RenderText_Blended(_font->get(), _value.c_str(), 0, _color));
    } catch (const std::string &err) {
      SDL_Log("%s",
              std::format("Text@{} Failed to change value from {} to: {}\n  {}",
                          (void *)this, _value, value, err.c_str())
                  .c_str());
    }
  }

  void setColor(const SDL_Color &color) {
    try {
      replaceSurface(
          TTF_RenderText_Blended(_font->get(), _value.c_str(), 0, color));
      _color = color;
    } catch (const std::string &err) {
      SDL_Log("%s",
              std::format("Text@{} Failed to change color from R:{} G:{} B:{} "
                          "A:{} to: R:{} G:{} B:{} A:{}\n  {}",
                          (void *)this, _color.r, _color.g, _color.b, _color.a,
                          color.r, color.g, color.b, color.a, err.c_str())
                  .c_str());
    }
  }

  void replaceFont(Font &font) {
    try {
      TTF_Text *text = TTF_CreateText(nullptr, font.get(), _value.c_str(), 0);
      if (!text) {
        std::string err{SDL_GetError()};
        if (!err.empty()) {
          err = std::format("Text@{} Failed to create: TTF_Text@{}\n  {}",
                            (void *)this, (void *)text, err);
          SDL_ClearError();
          throw err;
        }

        throw std::format("Text@{} Failed to create: TTF_Text@{}", (void *)this,
                          (void *)text);
      }
      TTF_DestroyText(_text);
      _font = &font;
      _text = text;
      replaceSurface(
          TTF_RenderText_Blended(_font->get(), _value.c_str(), 0, _color));
    } catch (const std::string &err) {
      SDL_Log("%s",
              std::format("Text@{} Failed to replace font with: Font@{}\n  {}",
                          (void *)this, (void *)&font, err.c_str())
                  .c_str());
    }
  }

  Text(const Text &) = delete;
  Text &operator=(const Text &) = delete;
};
