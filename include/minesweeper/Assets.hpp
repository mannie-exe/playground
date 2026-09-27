#pragma once

#include <assets/AssetCatalog.hpp>

namespace playground::minesweeper {
inline const assets::AssetId<assets::FontAsset> fontAsset{"minesweeper.font"};
inline const assets::AssetId<assets::VectorAsset> bombAsset{"minesweeper.bomb"};
inline const assets::AssetId<assets::VectorAsset> flagAsset{"minesweeper.flag"};

inline void registerAssets(assets::AssetCatalog &catalog) {
  catalog.add(fontAsset, assets::FontAsset{{"fonts/jurriaan_3d-fill.ttf"}});
  catalog.add(bombAsset, assets::VectorAsset{assets::FileSource{
                             "minesweeper/images/bomb.svg"}});
  catalog.add(flagAsset, assets::VectorAsset{assets::FileSource{
                             "minesweeper/images/flag.svg"}});
}
} // namespace playground::minesweeper
