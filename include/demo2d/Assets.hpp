#pragma once

#include <app/Assets.hpp>
#include <assets/AssetCatalog.hpp>
#include <platform/sdl/AssetResources.hpp>

namespace playground::demo2d {
inline const assets::AssetId<assets::ImageAsset> imageAsset{"demo2d.image"};
inline const assets::AssetId<assets::VectorAsset> addIconAsset{"demo2d.add"};
inline const assets::AssetId<assets::VectorAsset> removeIconAsset{
    "demo2d.remove"};

inline void registerAssets(assets::AssetCatalog &catalog) {
  catalog.add(imageAsset,
              assets::ImageAsset{assets::FileSource{"images/IMG_6239.PNG"}});
  catalog.add(addIconAsset,
              assets::VectorAsset{assets::FileSource{"ui/icons/add.svg"}});
  catalog.add(removeIconAsset,
              assets::VectorAsset{assets::FileSource{"ui/icons/remove.svg"}});
}

struct ViewResources {
  SurfaceHandle image;
  FontHandle font;
  SVGDocumentHandle addIcon, removeIcon;
};

inline ViewResources acquireResources(sdl::AssetResources &assets,
                                      float fontSize) {
  return {assets.image(imageAsset),
          assets.font(app::fontAsset, {.style = {.size = fontSize}}),
          assets.vector(addIconAsset), assets.vector(removeIconAsset)};
}
} // namespace playground::demo2d
