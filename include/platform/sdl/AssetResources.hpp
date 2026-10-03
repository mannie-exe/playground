#pragma once

#include <memory>
#include <thread>
#include <unordered_map>

#include <assets/AssetCatalog.hpp>
#include <platform/sdl/AssetPreparation.hpp>
#include <support/AssetRegistry.hpp>

namespace playground::sdl {

struct AssetResourceProps {
  std::size_t maxPreparedModels{8}, maxPreparedTextures{8},
      maxPreparedEnvironments{2};
  std::size_t maxImageBytes{128 * 1024 * 1024};
  std::size_t maxDecodedImageBytes{256 * 1024 * 1024};
  std::size_t maxSVGBytes{16 * 1024 * 1024};
  void validate() const;
};

// Owner-thread adapter. Catalog outlives requests through shared const
// ownership; the borrowed SDL cache must outlive this service and every using
// view.
class AssetResources {
  const std::thread::id _owner{std::this_thread::get_id()};
  AssetResourceProps _props;
  std::shared_ptr<const assets::AssetCatalog> _catalog;
  AssetRegistry &_cache;
  std::unordered_map<std::string, SVGDocumentHandle> _vectors;

  template <class T> struct PreparedEntry {
    std::shared_ptr<const T> value;
    std::uint64_t lastUse{};
  };

  std::uint64_t _clock{};
  std::unordered_map<std::string, PreparedEntry<scene::ModelAsset>> _models;
  std::unordered_map<std::string, PreparedEntry<rendering::Texture>> _textures;
  std::unordered_map<std::string, PreparedEntry<EnvironmentResource>>
      _environments;
  std::unordered_map<std::string, std::shared_ptr<const assets::CompiledShader>>
      _shaders;
  void checkOwner() const;

public:
  AssetResources(std::shared_ptr<const assets::AssetCatalog>, AssetRegistry &,
                 AssetResourceProps = {});

  const AssetResourceProps &props() const noexcept { return _props; }

  const assets::AssetCatalog &catalog() const noexcept { return *_catalog; }

  std::shared_ptr<const assets::AssetCatalog> catalogHandle() const {
    return _catalog;
  }

  AssetRegistry &cache() const {
    checkOwner();
    return _cache;
  }

  FontHandle font(const assets::AssetId<assets::FontAsset> &,
                  FontProps props = {});
  SurfaceHandle image(const assets::AssetId<assets::ImageAsset> &);
  SVGDocumentHandle vector(const assets::AssetId<assets::VectorAsset> &);
  scene::MeshHandle mesh(const assets::AssetId<assets::MeshAsset> &) const;
  scene::ModelHandle model(const assets::AssetId<assets::ModelAsset> &);
  scene::ModelHandle publishModel(const PreparedModel &);
  std::shared_ptr<const assets::CompiledShader>
  shader(const assets::AssetId<assets::ShaderAsset> &);
  // Keys include the frozen catalog generation and every preparation option.
  std::string key(const TextureRequest &) const;
  std::string key(const EnvironmentRequest &) const;
  PreparedAssets cached(const AssetPreparationProps &);
  PreparedAssets publish(PreparedAssets, const AssetPreparationProps &);
  void trimUnused();
  void trimRetained();
  void reclaim(bool pressure, std::size_t fonts, std::size_t images,
               std::size_t vectors, std::size_t surfaceBytes);
};

} // namespace playground::sdl
