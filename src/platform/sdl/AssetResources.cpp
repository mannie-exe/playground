#include <stdexcept>

#include <SDL3/SDL_iostream.h>
#include <SDL3_image/SDL_image.h>

#include <platform/sdl/AssetResources.hpp>

namespace playground::sdl {
void AssetResourceProps::validate() const {
  if (!maxImageBytes || !maxDecodedImageBytes || !maxSVGBytes)
    throw std::invalid_argument("Asset resource limits must be positive");
}

void AssetResources::checkOwner() const {
  if (_owner != std::this_thread::get_id())
    throw std::logic_error(
        "Resource acquisition/publication requires its owner thread");
}

AssetResources::AssetResources(
    std::shared_ptr<const assets::AssetCatalog> catalog, AssetRegistry &cache,
    AssetResourceProps props)
    : _props{props}, _catalog{std::move(catalog)}, _cache{cache} {
  props.validate();
  if (!_catalog || !_catalog->isFrozen())
    throw std::invalid_argument("Asset resources require a frozen catalog");
}

FontHandle AssetResources::font(const assets::AssetId<assets::FontAsset> &id,
                                FontProps props) {
  checkOwner();
  const auto path =
      _catalog->resolve(_catalog->definition(id).source).u8string();
  props.path.assign(reinterpret_cast<const char *>(path.data()), path.size());
  props.cacheIdentity = _catalog->cacheKey(assets::key(id));
  return _cache.getFont(std::move(props));
}

SurfaceHandle
AssetResources::image(const assets::AssetId<assets::ImageAsset> &id) {
  checkOwner();
  const auto cacheKey =
      std::string{"\0asset:", 7} + _catalog->cacheKey(assets::key(id));
  auto result = _cache.getImage(cacheKey, [&] {
    const auto bytes =
        _catalog->read(_catalog->definition(id).source, _props.maxImageBytes);
    using Stream = SDLResource<SDL_IOStream, SDL_CloseIO>;
    Stream stream{requireSDL(SDL_IOFromConstMem(bytes.data(), bytes.size()),
                             "Open image asset")};
    auto image = adoptManagedSurface(
        requireSDL(IMG_Load_IO(stream.get(), false), "Decode image asset"));
    if (image->pitch < 0 || image->h < 0 ||
        static_cast<std::uint64_t>(image->pitch) * image->h >
            _props.maxDecodedImageBytes)
      throw std::length_error("Decoded asset image exceeds resident budget");
    return image;
  });
  if (result->pitch < 0 || result->h < 0 ||
      static_cast<std::uint64_t>(result->pitch) * result->h >
          _props.maxDecodedImageBytes)
    throw std::length_error("Cached asset image exceeds resident budget");
  return result;
}

SVGDocumentHandle
AssetResources::vector(const assets::AssetId<assets::VectorAsset> &id) {
  checkOwner();
  const auto cacheKey = _catalog->cacheKey(assets::key(id));
  if (const auto it = _vectors.find(cacheKey); it != _vectors.end())
    return it->second;
  const auto bytes =
      _catalog->read(_catalog->definition(id).source, _props.maxSVGBytes);
  auto result = std::make_shared<const SVGDocument>(
      std::string{reinterpret_cast<const char *>(bytes.data()), bytes.size()});
  _vectors.emplace(cacheKey, result);
  return result;
}

scene::MeshHandle
AssetResources::mesh(const assets::AssetId<assets::MeshAsset> &id) const {
  checkOwner();
  return _catalog->definition(id).mesh;
}

void AssetResources::trimUnused() {
  checkOwner();
  std::erase_if(_vectors, [](const auto &entry) {
    return entry.second.use_count() == 1;
  });
  std::erase_if(
      _models, [](const auto &entry) { return entry.second.use_count() == 1; });
  std::erase_if(_shaders, [](const auto &entry) {
    return entry.second.use_count() == 1;
  });
}

scene::ModelHandle
AssetResources::model(const assets::AssetId<assets::ModelAsset> &id) {
  checkOwner();
  const auto key = _catalog->cacheKey(assets::key(id));
  if (const auto it = _models.find(key); it != _models.end())
    return it->second;
  auto model = prepareModel(*_catalog, id);
  _models.emplace(key, model);
  return model;
}

scene::ModelHandle
AssetResources::publishModel(const ModelPreparationResult &result) {
  checkOwner();
  if (result.error)
    std::rethrow_exception(result.error);
  if (!result.model ||
      result.definitionKey != _catalog->cacheKey(assets::key(result.asset)))
    throw std::invalid_argument(
        "Model result belongs to another catalog definition");
  return _models.emplace(result.definitionKey, result.model).first->second;
}

std::shared_ptr<const assets::CompiledShader>
AssetResources::shader(const assets::AssetId<assets::ShaderAsset> &id) {
  checkOwner();
  const auto key = _catalog->cacheKey(assets::key(id));
  if (const auto it = _shaders.find(key); it != _shaders.end())
    return it->second;
  auto shader = std::make_shared<const assets::CompiledShader>(
      assets::prepareShader(*_catalog, id));
  _shaders.emplace(key, shader);
  return shader;
}
} // namespace playground::sdl
