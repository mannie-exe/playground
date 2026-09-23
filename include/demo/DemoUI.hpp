#pragma once

#include <demo/Config.hpp>
#include <memory>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <ui/Builders.hpp>
#include <ui/content/Image.hpp>
#include <ui/content/Text.hpp>

namespace playground::demo {

inline std::unique_ptr<ui::Node>
makeDemoUI(AssetRegistry &assets, SurfaceHandle image, FontHandle font) {
  auto layers = ui::make<ui::ZStack>();
  layers->append(ui::make<ui::Image>(ui::ImageProps{
      .image = sdl::makeSurfaceImage(std::move(image)),
      .content = {.fit = ui::ContentFit::None, .alignment = {}}}));
  layers->append(ui::make<ui::Text>(
      assets, ui::TextProps{.value = std::string{config::textValue},
                            .font = std::move(font),
                            .foreground = config::textColor,
                            .fontFit = ui::FontFit::ShrinkToFit}));

  return ui::makeBox({.padding = layout::Insets::all(24)}, std::move(layers),
                     {.contentAlignment = layout::Alignment::stretch()});
}

} // namespace playground::demo
