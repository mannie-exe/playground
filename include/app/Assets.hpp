#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <assets/AssetCatalog.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <ui/Theme.hpp>

namespace playground::app {
inline const assets::AssetId<assets::FontAsset> fontAsset{
    "app.font.inter.400.upright.text"};

struct ThemeFontAsset {
  ui::FontFamily family;
  assets::AssetId<assets::FontAsset> id;
  std::string path;
  ui::FontSelection selection;
};

inline const std::vector<ThemeFontAsset> &themeFontAssets() {
  static const auto value = [] {
    std::vector<ThemeFontAsset> result;
    struct Weight {
      int value;
      std::string_view suffix;
    };
    constexpr std::array weights{
        Weight{100, "Thin"},   Weight{200, "ExtraLight"},
        Weight{300, "Light"},  Weight{400, "Regular"},
        Weight{500, "Medium"}, Weight{600, "SemiBold"},
        Weight{700, "Bold"},   Weight{800, "ExtraBold"},
        Weight{900, "Black"}};
    constexpr std::array names{"inter", "inter-display", "jetbrains-mono",
                               "source-serif-4"};
    constexpr std::array folders{"Inter/Inter", "Inter/InterDisplay",
                                 "JetBrainsMono/JetBrainsMono",
                                 "SourceSerif4/SourceSerif4"};
    constexpr std::array opticalNames{"text", "caption", "small-text",
                                      "subhead", "display"};
    constexpr std::array opticalSuffixes{"", "Caption", "SmText", "Subhead",
                                         "Display"};
    for (unsigned family = 0; family < names.size(); ++family) {
      const bool serif = family == static_cast<unsigned>(ui::FontFamily::Serif);
      for (unsigned optical = 0; optical < (serif ? opticalNames.size() : 1);
           ++optical)
        for (auto weight : weights) {
          if ((serif && (weight.value == 100 || weight.value == 500 ||
                         weight.value == 800)) ||
              (family == static_cast<unsigned>(ui::FontFamily::Monospace) &&
               weight.value == 900))
            continue;
          for (bool italic : {false, true}) {
            std::string suffix{weight.suffix};
            if (serif && weight.value == 600)
              suffix = "Semibold";
            if (italic) {
              if (weight.value == 400)
                suffix.clear();
              suffix += serif ? "It" : "Italic";
            }
            const auto id = std::string{"app.font."} + names[family] + "." +
                            std::to_string(weight.value) +
                            (italic ? ".italic." : ".upright.") +
                            opticalNames[optical];
            result.push_back(
                {static_cast<ui::FontFamily>(family),
                 {id},
                 std::string{"fonts/"} + folders[family] +
                     opticalSuffixes[optical] + "-" + suffix + ".ttf",
                 {weight.value,
                  italic ? ui::FontSlant::Italic : ui::FontSlant::Upright,
                  static_cast<ui::FontOpticalSize>(optical)}});
          }
        }
    }
    return result;
  }();
  return value;
}

inline void registerAssets(assets::AssetCatalog &catalog) {
  for (const auto &font : themeFontAssets())
    catalog.add(font.id, assets::FontAsset{{font.path}});
}

inline void configureThemeFonts(ui::ThemeTypography &typography,
                                sdl::AssetResources &resources) {
  constexpr std::array names{"Inter", "Inter Display", "JetBrains Mono",
                             "Source Serif 4"};
  const auto &catalog = resources.catalog();
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (typography.families[index])
      continue;
    std::vector<ui::FontFace> faces;
    for (const auto &font : themeFontAssets()) {
      if (static_cast<std::size_t>(font.family) != index)
        continue;
      const auto path = catalog.resolve(catalog.definition(font.id).source)
                            .generic_u8string();
      faces.push_back(
          {font.id.value,
           {reinterpret_cast<const char *>(path.data()), path.size()},
           catalog.cacheKey(assets::key(font.id)),
           font.selection});
    }
    typography.families[index] =
        std::make_shared<const ui::FontFamilyDefinition>(names[index],
                                                         std::move(faces));
  }
}
} // namespace playground::app
