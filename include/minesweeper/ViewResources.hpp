#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <minesweeper/Assets.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <support/SDLError.hpp>
#include <ui/content/Text.hpp>
#include <ui/content/Vector.hpp>

namespace playground::minesweeper {

struct ViewResources {
  AssetRegistry &registry;
  FontHandle font;
  SVGDocumentHandle bomb;
  SVGDocumentHandle flag;
};

inline ViewResources acquireResources(sdl::AssetResources &assets) {
  return {assets.cache(), assets.font(fontAsset, {.style = {.size = 32}}),
          assets.vector(bombAsset), assets.vector(flagAsset)};
}

inline FontHandle fittedFont(const ViewResources &resources,
                             const std::string &value, math::Size2 box) {
  if (!resources.font)
    throw std::invalid_argument("Minesweeper requires a font");
  if (value.empty() || !math::hasArea(box))
    return resources.font;
  int width{}, height{};
  if (!TTF_GetStringSize(resources.font->get(), value.c_str(), value.size(),
                         &width, &height))
    throwSDLError("TTF_GetStringSize");
  if (width <= 0 || height <= 0)
    return resources.font;
  auto props = resources.font->props();
  // Preserve the original fit: enlargement as well as shrinking, on both axes.
  props.style.size =
      std::max(1.0f, props.style.size * std::min(box.width * 0.825f / width,
                                                 box.height * 0.825f / height));
  return resources.registry.getFont(std::move(props));
}

inline std::unique_ptr<ui::Text> makeLabel(const ViewResources &resources,
                                           std::string value,
                                           math::ColorRGBA8 color,
                                           math::Size2 box) {
  auto font = fittedFont(resources, value, box);
  return std::make_unique<ui::Text>(
      resources.registry,
      ui::TextProps{.value = std::move(value),
                    .font = std::move(font),
                    .foreground = color,
                    .contentAlignment = layout::Alignment::center(),
                    .useTheme = false});
}

inline std::unique_ptr<ui::Vector>
makeIcon(const ViewResources &resources, bool bomb,
         math::ColorRGBA8 tint = {255, 255, 255, 255}) {
  return std::make_unique<ui::Vector>(
      resources.registry,
      ui::VectorProps{.source = bomb ? resources.bomb : resources.flag,
                      .content = {.paint = {.tint = tint}}});
}

} // namespace playground::minesweeper
