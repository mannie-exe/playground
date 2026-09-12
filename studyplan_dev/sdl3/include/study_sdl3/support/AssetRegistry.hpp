#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

#include <study_sdl3/support/Font.hpp>
#include <study_sdl3/support/SDLError.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>
#include <study_sdl3/support/SDLResource.hpp>
#include <study_sdl3/support/SurfaceHandles.hpp>

class AssetRegistry {
  using StreamResource = SDLResource<SDL_IOStream, SDL_CloseIO>;

  template <typename T> struct Entry {
    T value;
    std::uint64_t lastUse{};
  };

  std::unordered_map<std::string, Entry<FontHandle>> _fonts;
  std::unordered_map<std::string, Entry<SurfaceHandle>> _images;
  std::unordered_map<std::string, Entry<SurfaceHandle>> _vectors;
  std::uint64_t _clock{};

  static SurfaceHandle ownSurface(SDL_Surface *surface) {
    return SurfaceHandle{surface, SurfaceHandleDeleter{}};
  }

  static std::string vectorKey(const std::string &path, Vec2i size) {
    return path + "\n" + std::to_string(size.x) + "," + std::to_string(size.y);
  }

  static SurfaceHandle loadVector(const std::string &path, Vec2i size) {
    StreamResource stream{SDL_IOFromFile(path.c_str(), "rb")};
    if (!stream)
      throwSDLError("AssetRegistry failed to open SVG: " + path);

    SDL_Surface *surface{hasArea(size)
                             ? IMG_LoadSizedSVG_IO(stream.get(), size.x, size.y)
                             : IMG_LoadSVG_IO(stream.get())};
    if (!surface)
      throwSDLError("AssetRegistry failed to load SVG: " + path);
    return ownSurface(surface);
  }

public:
  FontHandle getFont(FontProps props) {
    const std::string key{props.path + "\n" + std::to_string(props.style.size) +
                          ":" + std::to_string(props.style.flags) + ":" +
                          std::to_string(props.style.outline) + ":" +
                          std::to_string(props.layout.alignment) + ":" +
                          std::to_string(props.layout.direction) + ":" +
                          std::to_string(props.layout.lineSpace) + ":" +
                          std::to_string(props.render.hinting) + ":" +
                          std::to_string(props.render.sdf) + ":" +
                          std::to_string(props.render.kern)};
    if (const auto it{_fonts.find(key)}; it != _fonts.end()) {
      it->second.lastUse = ++_clock;
      return it->second.value;
    }

    auto font{std::make_shared<Font>(std::move(props))};
    FontHandle result{font};
    _fonts.emplace(key, Entry<FontHandle>{result, ++_clock});
    return result;
  }

  SurfaceHandle getImage(const std::string &path) {
    if (const auto it{_images.find(path)}; it != _images.end()) {
      it->second.lastUse = ++_clock;
      return it->second.value;
    }

    SurfaceHandle surface{
        ownSurface(requireSDL(IMG_Load(path.c_str()),
                              "AssetRegistry failed to load image: " + path))};
    _images.emplace(path, Entry<SurfaceHandle>{surface, ++_clock});
    return surface;
  }

  SurfaceHandle getVector(const std::string &path, Vec2i size = {}) {
    const std::string key{vectorKey(path, size)};
    if (const auto it{_vectors.find(key)}; it != _vectors.end()) {
      it->second.lastUse = ++_clock;
      return it->second.value;
    }

    SurfaceHandle surface{loadVector(path, size)};
    _vectors.emplace(key, Entry<SurfaceHandle>{surface, ++_clock});
    return surface;
  }

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
            std::size_t maximumVectors) {
    trimMap(_fonts, maximumFonts);
    trimMap(_images, maximumImages);
    trimMap(_vectors, maximumVectors);
  }

  void clear() {
    _vectors.clear();
    _images.clear();
    _fonts.clear();
  }
};
