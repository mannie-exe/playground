#include <format>
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
        requireSDL(IMG_Load_IO(stream.get(), false), "Decode image asset"),
        _cache.resources());
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
  std::erase_if(_models, [](const auto &entry) {
    return entry.second.value.use_count() == 1;
  });
  std::erase_if(_textures, [](const auto &entry) {
    return entry.second.value.use_count() == 1;
  });
  std::erase_if(_environments, [](const auto &entry) {
    return entry.second.value.use_count() == 1;
  });
  std::erase_if(_shaders, [](const auto &entry) {
    return entry.second.use_count() == 1;
  });
}

scene::ModelHandle
AssetResources::model(const assets::AssetId<assets::ModelAsset> &id) {
  checkOwner();
  const auto key = _catalog->cacheKey(assets::key(id));
  if (const auto it = _models.find(key); it != _models.end()) {
    it->second.lastUse = ++_clock;
    return it->second.value;
  }
  auto model = prepareModel(*_catalog, id, {}, _cache.resources());
  _models.emplace(key, PreparedEntry<scene::ModelAsset>{model, ++_clock});
  return model;
}

scene::ModelHandle AssetResources::publishModel(const PreparedModel &result) {
  checkOwner();
  if (!result.model ||
      result.definitionKey != _catalog->cacheKey(assets::key(result.asset)))
    throw std::invalid_argument(
        "Model result belongs to another catalog definition");
  auto &entry =
      _models
          .try_emplace(result.definitionKey,
                       PreparedEntry<scene::ModelAsset>{result.model, 0})
          .first->second;
  entry.lastUse = ++_clock;
  return entry.value;
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

std::string AssetResources::key(const TextureRequest &request) const {
  if (!request.maximumBytes)
    throw std::invalid_argument("Texture input cap must be positive");
  return std::format("{}:texture:{}:{}:{}",
                     _catalog->cacheKey(assets::key(request.asset)),
                     int(request.role), int(request.mip), request.maximumBytes);
}

std::string AssetResources::key(const EnvironmentRequest &request) const {
  request.props.validate();
  if (!request.maximumBytes)
    throw std::invalid_argument("Environment input cap must be positive");
  const auto &p = request.props;
  return std::format("{}:environment:{}:{}:{}:{}:{}",
                     _catalog->cacheKey(assets::key(request.asset)),
                     p.diffuseWidth, p.specularWidth, p.brdfSize, p.samples,
                     request.maximumBytes);
}

PreparedAssets AssetResources::cached(const AssetPreparationProps &props) {
  checkOwner();
  PreparedAssets result;
  for (const auto &id : props.models) {
    const auto key = _catalog->cacheKey(assets::key(id));
    scene::ModelHandle model;
    if (auto it = _models.find(key); it != _models.end()) {
      it->second.lastUse = ++_clock;
      model = it->second.value;
    }
    result.models.push_back({id, key, std::move(model)});
  }
  for (const auto &request : props.textures) {
    const auto name = key(request);
    rendering::TextureHandle texture;
    if (auto it = _textures.find(name); it != _textures.end()) {
      it->second.lastUse = ++_clock;
      texture = it->second.value;
    }
    result.textures.emplace_back(name, std::move(texture));
  }
  if (props.environment) {
    result.environmentKey = key(*props.environment);
    if (auto it = _environments.find(result.environmentKey);
        it != _environments.end()) {
      it->second.lastUse = ++_clock;
      result.environment = it->second.value;
    }
  }
  return result;
}

PreparedAssets AssetResources::publish(PreparedAssets result,
                                       const AssetPreparationProps &props) {
  checkOwner();
  if (result.models.size() != props.models.size() ||
      result.textures.size() != props.textures.size() ||
      bool(result.environment) != bool(props.environment) ||
      (props.environment && result.environmentKey != key(*props.environment)))
    throw std::invalid_argument("Prepared assets do not match request");
  if (result.environment &&
      (!result.environment->source || !result.environment->lighting.diffuse ||
       !result.environment->lighting.specular ||
       !result.environment->lighting.brdf))
    throw std::invalid_argument("Incomplete prepared environment");
  // Validate the entire batch before publishing any cache entry.
  for (std::size_t i = 0; i < props.models.size(); ++i)
    if (!result.models[i].model || result.models[i].asset != props.models[i] ||
        result.models[i].definitionKey !=
            _catalog->cacheKey(assets::key(props.models[i])))
      throw std::invalid_argument("Prepared model belongs to another request");
  for (std::size_t i = 0; i < props.textures.size(); ++i)
    if (!result.textures[i].second ||
        result.textures[i].first != key(props.textures[i]))
      throw std::invalid_argument(
          "Prepared texture belongs to another request");
  for (auto &model : result.models)
    model.model = publishModel(model);
  for (auto &[key, texture] : result.textures) {
    auto &entry =
        _textures
            .try_emplace(key, PreparedEntry<rendering::Texture>{texture, 0})
            .first->second;
    entry.lastUse = ++_clock;
    texture = entry.value;
  }
  if (result.environment) {
    auto &entry = _environments
                      .try_emplace(result.environmentKey,
                                   PreparedEntry<EnvironmentResource>{
                                       result.environment, 0})
                      .first->second;
    entry.lastUse = ++_clock;
    result.environment = entry.value;
  }
  trimRetained();
  return result;
}

void AssetResources::reclaim(bool pressure, std::size_t fonts,
                             std::size_t images, std::size_t vectors,
                             std::size_t surfaceBytes) {
  checkOwner();
  _cache.trim(pressure ? 0 : fonts, pressure ? 0 : images,
              pressure ? 0 : vectors);
  _cache.trimSurfaceBytes(pressure ? 0 : surfaceBytes);
  if (pressure)
    trimUnused();
  else
    trimRetained();
}

void AssetResources::trimRetained() {
  checkOwner();
  std::erase_if(_vectors, [](const auto &entry) {
    return entry.second.use_count() == 1;
  });
  std::erase_if(_shaders, [](const auto &entry) {
    return entry.second.use_count() == 1;
  });
  AssetRegistry::trimMap(_models, _props.maxPreparedModels);
  AssetRegistry::trimMap(_textures, _props.maxPreparedTextures);
  AssetRegistry::trimMap(_environments, _props.maxPreparedEnvironments);
}
} // namespace playground::sdl
