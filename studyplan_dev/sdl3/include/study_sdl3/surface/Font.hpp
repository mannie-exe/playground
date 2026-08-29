#pragma once

#include <format>
#include <string>
#include <string_view>

#include <SDL3_ttf/SDL_ttf.h>

#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLResource.hpp>

// TODO: create font registry for caching with good invalidation to minimize
// size and maximize efficiency

class Font {
  SDLResource<TTF_Font, TTF_CloseFont> _font;
  std::string _path;
  float _size;

public:
  Font(const std::string &path, const float size)
      : _font{TTF_OpenFont(path.c_str(), size)}, _path{path}, _size{size} {
    if (!_font) {
      throwSDLError(
          std::format("Font@{} Failed to load {}", (void *)this, path));
    }
  };

  TTF_Font *get() const { return _font.get(); }
  std::string_view getPath() const { return _path; }
  float getSize() const { return _size; }

  void setSize(float size) {
    if (!TTF_SetFontSize(_font.get(), size)) {
      throwSDLError(
          std::format("Font@{} Failed to update TTF_Font@{} size from {} to {}",
                      (void *)this, (void *)_font.get(), _size, size));
    }

    _size = size;
  }

  Font cloneWithSize(float size) const { return Font(_path, size); }

  Font(Font &&) noexcept = default;
  Font &operator=(Font &&) noexcept = default;

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;
};
