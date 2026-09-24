#pragma once

#include <string>
#include <utility>

#include <rendering/TextImagePreparer.hpp>
#include <support/Font.hpp>

namespace playground::sdl {

// Exact resolved font/text/wrap produced by layout, not another layout policy.
struct FontTextSource final : rendering::TextSource {
  const FontHandle font;
  const std::string value;
  const int wrapWidth;
  const math::ColorRGBA8 foreground;

  FontTextSource(FontHandle font, std::string value, int wrapWidth,
                 math::ColorRGBA8 foreground)
      : font{std::move(font)}, value{std::move(value)}, wrapWidth{wrapWidth},
        foreground{foreground} {}
};

} // namespace playground::sdl
