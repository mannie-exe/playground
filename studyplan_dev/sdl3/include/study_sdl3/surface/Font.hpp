#pragma once

#include <format>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3_ttf/SDL_ttf.h>

class Font {
  TTF_Font *_font{nullptr};
  float _size;

public:
  Font(const char *path, float size)
      : _font{TTF_OpenFont(path, size)}, _size(size) {
    if (!_font) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format("Font@{} Failed to load: {}\n  {}\n  TTF_Font@{}",
                          (void *)this, path, err.c_str(), (void *)_font);
        SDL_ClearError();
        throw err;
      }

      throw std::format("Font@{} Failed to load", (void *)this);
    }
  };
  ~Font() { TTF_CloseFont(_font); }

  TTF_Font *get() const { return _font; }
  float getSize() const { return _size; }

  void setSize(float size) {
    if (!TTF_SetFontSize(_font, size)) {
      std::string err{SDL_GetError()};
      if (!err.empty()) {
        err = std::format(
            "Font@{} Failed to update TTF_Font@{} size from {} to: {}\n  {}",
            (void *)this, (void *)_font, _size, size, err.c_str());
        SDL_ClearError();
        throw err;
      }

      throw std::format(
          "Font@{} Failed to update TTF_Font@{} size from {} to: {}",
          (void *)this, (void *)_font, _size, size);
    }

    _size = size;
  }

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;
};
