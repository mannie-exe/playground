#pragma once

#include <memory>

#include <demo2d/Assets.hpp>
#include <demo2d/Config.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <ui/Builders.hpp>
#include <ui/content/Image.hpp>
#include <ui/content/Text.hpp>

namespace playground::demo2d {

struct ViewProps {
  std::string text{config::textValue};
  math::ColorRGBA8 textColor{config::textColor};
  float padding{24};
};

inline std::unique_ptr<ui::Node>
makeDemo2DPreview(AssetRegistry &assets, const ViewResources &resources,
                  const ViewProps &props) {
  auto layers = ui::make<ui::ZStack>();
  layers->append(ui::make<ui::Image>(
      ui::ImageProps{.image = sdl::makeSurfaceImage(resources.image),
                     .content = {.fit = ui::ContentFit::Contain,
                                 .alignment = layout::Alignment::center()}}));
  layers->append(ui::make<ui::Text>(
      assets,
      ui::TextProps{.value = props.text,
                    .font = resources.font,
                    .foreground = props.textColor,
                    .fontFit = ui::FontFit::ShrinkToFit,
                    .colorTreatment = ui::ColorTreatment::PreserveArtwork}));

  return ui::makeBox({.padding = layout::Insets::all(props.padding)},
                     std::move(layers),
                     {.contentAlignment = layout::Alignment::stretch()});
}

std::unique_ptr<ui::Node> makeDemo2DUI(AssetRegistry &, const ViewResources &,
                                       const ViewProps &);

} // namespace playground::demo2d
