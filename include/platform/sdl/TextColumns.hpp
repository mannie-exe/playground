#pragma once

#include <layout/LayoutPrimitives.hpp>
#include <platform/sdl/SDLGeometry.hpp>
#include <support/AssetRegistry.hpp>
#include <support/TextFlow.hpp>

namespace playground::sdl {

struct TextColumnRun {
  FontHandle font;
  std::string value;
  math::Rect bounds;
  bool sideways{};
};

struct TextColumns {
  math::Size2 size;
  std::size_t count{};
  std::vector<TextColumnRun> runs;
};

bool sidewaysCluster(std::string_view cluster, ui::TextOrientation orientation);

// Wrap and shape each final run, not isolated codepoints. Column breaks are
// extended-grapheme-safe; language-specific line-breaking rules are separate.
TextColumns layoutTextColumns(AssetRegistry &assets, FontHandle font,
                              std::string_view value,
                              std::optional<float> height, ui::WritingMode mode,
                              ui::TextOrientation orientation,
                              layout::Align alignment);

template <typename Rasterize>
SurfaceHandle
rasterizeTextColumns(const TextColumns &columns, Rasterize rasterize,
                     std::optional<math::ColorRGBA8> background = {}) {
  SurfaceHandle result{
      requireSDL(SDL_CreateSurface(
                     checkedPixel(columns.size.width, PixelRounding::Ceil),
                     checkedPixel(columns.size.height, PixelRounding::Ceil),
                     SDL_PIXELFORMAT_RGBA32),
                 "Create text columns"),
      SurfaceHandleDeleter{}};
  const auto color = background.value_or(math::ColorRGBA8{0, 0, 0, 0});
  if (!SDL_FillSurfaceRect(
          result.get(), nullptr,
          SDL_MapSurfaceRGBA(result.get(), color.r, color.g, color.b, color.a)))
    throwSDLError("Clear text columns");
  for (const auto &run : columns.runs) {
    auto source = rasterize(run.font, run.value);
    const int dx = checkedPixel(run.bounds.x()),
              dy = checkedPixel(run.bounds.y());
    // Exact quarter-turn copy retains straight alpha; no filtering or blending
    // on transparent intermediates that would premultiply a second time.
    for (int y = 0; y < source->h; ++y)
      for (int x = 0; x < source->w; ++x) {
        Uint8 r{}, g{}, b{}, a{};
        if (!SDL_ReadSurfacePixel(source.get(), x, y, &r, &g, &b, &a) ||
            !SDL_WriteSurfacePixel(result.get(),
                                   dx + (run.sideways ? source->h - 1 - y : x),
                                   dy + (run.sideways ? x : y), r, g, b, a))
          throwSDLError("Copy vertical text pixels");
      }
  }
  return result;
}

} // namespace playground::sdl
