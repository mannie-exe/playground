#pragma once

#include <filesystem>

#include <ui/FontFamily.hpp>

namespace playground::sdl {
// System fonts remain on the user's machine; they are never release assets.
inline ui::FontFamilyHandle systemEmojiFont() {
#ifdef __APPLE__
  constexpr auto path = "/System/Library/Fonts/Apple Color Emoji.ttc";
  std::error_code error;
  if (std::filesystem::is_regular_file(path, error))
    return std::make_shared<const ui::FontFamilyDefinition>(
        "Apple Color Emoji",
        std::vector<ui::FontFace>{{"system.apple-color-emoji", path, {}, {}}});
#endif
  return {};
}
} // namespace playground::sdl
