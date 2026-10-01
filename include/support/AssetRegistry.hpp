#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <math/Geometry2D.hpp>
#include <support/Font.hpp>
#include <support/SDLError.hpp>
#include <support/SDLResource.hpp>
#include <support/SVGDocument.hpp>
#include <support/SurfaceHandles.hpp>

class AssetRegistry {
  template <typename T> struct Entry {
    T value;
    std::uint64_t lastUse{};
  };

  std::unordered_map<std::string, Entry<FontHandle>> _fonts;
  std::unordered_map<std::string, Entry<SurfaceHandle>> _images;
  std::unordered_map<std::string, Entry<SurfaceHandle>> _vectors;
  std::unordered_map<std::string, Entry<SurfaceHandle>> _text;
  std::unordered_map<std::string, Entry<playground::SVGDocumentHandle>>
      _svgDocuments;
  std::uint64_t _clock{};

  static SurfaceHandle ownSurface(SDL_Surface *surface) {
    return adoptManagedSurface(surface);
  }

  static std::string vectorKey(const std::string &path,
                               playground::math::Vec2i size) {
    return path + "\n" + std::to_string(size.x) + "," + std::to_string(size.y);
  }

  static SurfaceHandle loadVector(const std::string &path,
                                  playground::math::Vec2i size);

public:
  playground::SVGDocumentHandle getSVGDocument(const std::string &path);

  SurfaceHandle getVector(const playground::VectorSource &source,
                          const playground::SVGStyleOverrides &styles,
                          playground::math::Vec2i size = {});
  static std::string fontKey(const FontProps &props);

  FontHandle getFont(FontProps props);

  SurfaceHandle getImage(const std::string &path);
  SurfaceHandle getImage(const std::string &key,
                         const std::function<SurfaceHandle()> &create);

  SurfaceHandle getVector(const std::string &path,
                          playground::math::Vec2i size = {});

  // Callers encode every raster dependency, including text bytes and font
  // props.
  SurfaceHandle getText(const std::string &key,
                        const std::function<SurfaceHandle()> &create);

  std::size_t estimatedSurfaceBytes() const noexcept;

  void trimSurfaceBytes(std::size_t budget);

  template <typename Map> static void trimMap(Map &map, std::size_t maximum) {
    while (map.size() > maximum) {
      auto candidate{map.end()};
      for (auto it{map.begin()}; it != map.end(); ++it) {
        if (it->second.value.use_count() != 1)
          continue;
        if (candidate == map.end() ||
            it->second.lastUse < candidate->second.lastUse)
          candidate = it;
      }
      if (candidate == map.end())
        return;
      map.erase(candidate);
    }
  }

  void trim(std::size_t maximumFonts, std::size_t maximumImages,
            std::size_t maximumVectors);

  void clear();
};
