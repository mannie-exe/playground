#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

#include <support/AssetRegistry.hpp>

using StreamResource = SDLResource<SDL_IOStream, SDL_CloseIO>;

SurfaceHandle AssetRegistry::loadVector(const std::string &path,
                                        playground::math::Vec2i size) {
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

playground::SVGDocumentHandle
AssetRegistry::getSVGDocument(const std::string &path) {
  if (auto it = _svgDocuments.find(path); it != _svgDocuments.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }
  StreamResource stream{
      requireSDL(SDL_IOFromFile(path.c_str(), "rb"), "Open SVG")};
  std::string xml;
  char buffer[4096];
  for (;;) {
    const auto count = SDL_ReadIO(stream.get(), buffer, sizeof(buffer));
    xml.append(buffer, count);
    if (xml.size() > 16 * 1024 * 1024)
      throw std::length_error("SVG document exceeds 16 MiB input budget");
    if (count < sizeof(buffer)) {
      if (SDL_GetIOStatus(stream.get()) == SDL_IO_STATUS_ERROR)
        throwSDLError("Read SVG document");
      break;
    }
  }
  auto document =
      std::make_shared<const playground::SVGDocument>(std::move(xml));
  _svgDocuments.emplace(
      path, Entry<playground::SVGDocumentHandle>{document, ++_clock});
  return document;
}

SurfaceHandle
AssetRegistry::getVector(const playground::VectorSource &source,
                         const playground::SVGStyleOverrides &styles,
                         playground::math::Vec2i size) {
  if (const auto *path = std::get_if<std::string>(&source);
      path && styles.empty())
    return getVector(*path, size);
  const auto document = std::holds_alternative<std::string>(source)
                            ? getSVGDocument(std::get<std::string>(source))
                            : std::get<playground::SVGDocumentHandle>(source);
  if (!document)
    throw std::invalid_argument("Vector requires an SVG document");
  const auto xml = document->styled(styles);
  const auto key = std::string{"\0svg:", 5} + std::to_string(xml.size()) + ":" +
                   xml + ":" + std::to_string(size.x) + "," +
                   std::to_string(size.y);
  if (auto it = _vectors.find(key); it != _vectors.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }
  StreamResource stream{requireSDL(SDL_IOFromConstMem(xml.data(), xml.size()),
                                   "SVG memory stream")};
  auto surface = ownSurface(requireSDL(
      hasArea(size) ? IMG_LoadSizedSVG_IO(stream.get(), size.x, size.y)
                    : IMG_LoadSVG_IO(stream.get()),
      "Rasterize styled SVG"));
  _vectors.emplace(key, Entry<SurfaceHandle>{surface, ++_clock});
  return surface;
}

std::string AssetRegistry::fontKey(const FontProps &props) {
  auto key = std::to_string(props.cacheIdentity.size()) + ":" +
             props.cacheIdentity + ":" + std::to_string(props.path.size()) +
             ":" + props.path + ":" +
             std::to_string(std::bit_cast<std::uint32_t>(props.style.size)) +
             ":" + std::to_string(props.style.flags) + ":" +
             std::to_string(props.style.outline) + ":" +
             std::to_string(props.layout.alignment) + ":" +
             std::to_string(props.layout.direction) + ":" +
             (props.layout.lineSpace ? std::to_string(*props.layout.lineSpace)
                                     : "auto") +
             ":" + std::to_string(props.render.hinting) + ":" +
             std::to_string(props.render.sdf) + ":" +
             std::to_string(props.render.kern);
  for (const auto &font : props.fallbacks)
    key += ":" + std::to_string(font.path.size()) + ":" + font.path + ":" +
           std::to_string(font.cacheIdentity.size()) + ":" + font.cacheIdentity;
  return key;
}

FontHandle AssetRegistry::getFont(FontProps props) {
  if (!std::isfinite(props.style.size) || props.style.size <= 0)
    throw std::invalid_argument("Font size must be finite and positive");
  const std::string key = fontKey(props);
  if (const auto it{_fonts.find(key)}; it != _fonts.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }

  std::vector<FontHandle> fallbacks;
  for (const auto &source : props.fallbacks) {
    if (source.path.empty() || source.path.find('\0') != std::string::npos)
      throw std::invalid_argument("Invalid fallback font path");
    auto fallback = props;
    fallback.path = source.path;
    fallback.cacheIdentity = source.cacheIdentity;
    fallback.fallbacks.clear();
    fallbacks.push_back(getFont(std::move(fallback)));
  }
  FontHandle font{new Font(std::move(props), std::move(fallbacks), _ledger)};
  FontHandle result{font};
  _fonts.emplace(key, Entry<FontHandle>{result, ++_clock});
  return result;
}

SurfaceHandle AssetRegistry::getImage(const std::string &path) {
  return getImage(path, [&] {
    return ownSurface(requireSDL(
        IMG_Load(path.c_str()), "AssetRegistry failed to load image: " + path));
  });
}

SurfaceHandle
AssetRegistry::getImage(const std::string &key,
                        const std::function<SurfaceHandle()> &create) {
  if (const auto it{_images.find(key)}; it != _images.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }

  auto surface = create();
  if (!surface)
    throw std::invalid_argument("Image factory returned no surface");
  _images.emplace(key, Entry<SurfaceHandle>{surface, ++_clock});
  return surface;
}

SurfaceHandle AssetRegistry::getVector(const std::string &path,
                                       playground::math::Vec2i size) {
  const std::string key{vectorKey(path, size)};
  if (const auto it{_vectors.find(key)}; it != _vectors.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }

  SurfaceHandle surface{loadVector(path, size)};
  _vectors.emplace(key, Entry<SurfaceHandle>{surface, ++_clock});
  return surface;
}

SurfaceHandle
AssetRegistry::getText(const std::string &key,
                       const std::function<SurfaceHandle()> &create) {
  if (const auto it = _text.find(key); it != _text.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }
  auto surface = create();
  if (!surface)
    throw std::invalid_argument("Text factory returned no surface");
  _text.emplace(key, Entry<SurfaceHandle>{surface, ++_clock});
  return surface;
}

std::size_t AssetRegistry::estimatedSurfaceBytes() const noexcept {
  std::size_t bytes{};
  for (const auto *map : {&_images, &_vectors, &_text})
    for (const auto &[key, entry] : *map) {
      const auto amount =
          static_cast<std::uint64_t>(std::max(0, entry.value->pitch)) *
          std::max(0, entry.value->h);
      if (amount > std::numeric_limits<std::size_t>::max() - bytes)
        return std::numeric_limits<std::size_t>::max();
      bytes += static_cast<std::size_t>(amount);
    }
  return bytes;
}

void AssetRegistry::trimSurfaceBytes(std::size_t budget) {
  while (estimatedSurfaceBytes() > budget) {
    decltype(_images) *candidateMap{};
    decltype(_images)::iterator candidate;
    for (auto *map : {&_images, &_vectors, &_text})
      for (auto it = map->begin(); it != map->end(); ++it)
        if (it->second.value.use_count() == 1 &&
            (!candidateMap || it->second.lastUse < candidate->second.lastUse)) {
          candidateMap = map;
          candidate = it;
        }
    if (!candidateMap)
      return;
    candidateMap->erase(candidate);
  }
}

void AssetRegistry::trim(std::size_t maximumFonts, std::size_t maximumImages,
                         std::size_t maximumVectors) {
  trimMap(_fonts, maximumFonts);
  trimMap(_images, maximumImages);
  trimMap(_vectors, maximumVectors);
  trimMap(_svgDocuments, maximumVectors);
  trimMap(_text, maximumImages);
}

void AssetRegistry::clear() {
  _text.clear();
  _vectors.clear();
  _svgDocuments.clear();
  _images.clear();
  _fonts.clear();
}
