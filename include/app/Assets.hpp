#pragma once

#include <assets/AssetCatalog.hpp>

namespace playground::app {
inline const assets::AssetId<assets::FontAsset> fontAsset{"app.font"};

inline void registerAssets(assets::AssetCatalog &catalog) {
  catalog.add(fontAsset, assets::FontAsset{{"fonts/LBRITE.TTF"}});
}
} // namespace playground::app
