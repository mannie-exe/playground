#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <rendering/Shader.hpp>
#include <scene/ModelImport.hpp>

namespace playground::assets {

struct FileSource {
  std::filesystem::path path;
};

struct ByteSource {
  std::vector<std::byte> bytes;
};

using AssetSource = std::variant<FileSource, ByteSource>;

struct ImageAsset {
  AssetSource source;
};

struct FontAsset {
  FileSource source;
};

struct VectorAsset {
  AssetSource source;
};

struct BinaryAsset {
  AssetSource source;
};

struct MeshAsset {
  scene::MeshHandle mesh;
};

struct ShaderAsset {
  AssetSource source;
  rendering::ShaderStage stage;
  std::string entryPoint{"main"};
  rendering::ShaderLayout layout;
};

template <class T> struct AssetId {
  std::string value;
  bool operator==(const AssetId &) const = default;
};

// External glTF URIs map to explicitly registered binary assets. Embedded GLB
// resources need no mapping. No network or implicit sibling reads are
// performed.
struct ModelAsset {
  AssetSource source;
  scene::ModelImportProps props;
  std::map<std::string, AssetId<BinaryAsset>> resources;
};

using AssetDefinition =
    std::variant<ImageAsset, FontAsset, VectorAsset, BinaryAsset, MeshAsset,
                 ShaderAsset, ModelAsset>;

struct AssetKey {
  std::string value;
  std::size_t type;
  auto operator<=>(const AssetKey &) const = default;
};

template <class T, std::size_t I = 0> consteval std::size_t assetTypeIndex() {
  static_assert(I < std::variant_size_v<AssetDefinition>,
                "Unsupported asset definition");
  if constexpr (std::is_same_v<T,
                               std::variant_alternative_t<I, AssetDefinition>>)
    return I;
  else
    return assetTypeIndex<T, I + 1>();
}

template <class T> AssetKey key(const AssetId<T> &id) {
  return {id.value, assetTypeIndex<T>()};
}

struct AssetRecord {
  AssetDefinition definition;
  std::uint64_t revision{1};
  std::vector<AssetKey> dependencies;
};

// Register on one owner, then freeze. Frozen catalogs permit concurrent reads.
// Revision is an authored generation, never an automatic disk timestamp.
class AssetCatalog {
  const std::uint64_t _identity;
  std::filesystem::path _root;
  std::map<AssetKey, AssetRecord> _records;
  bool _frozen{};

  void addRecord(AssetKey, AssetRecord);

public:
  explicit AssetCatalog(std::filesystem::path root);
  AssetCatalog(const AssetCatalog &) = delete;
  AssetCatalog &operator=(const AssetCatalog &) = delete;

  template <class T>
  void add(AssetId<T> id, T definition, std::vector<AssetKey> dependencies = {},
           std::uint64_t revision = 1) {
    addRecord(key(id),
              {std::move(definition), revision, std::move(dependencies)});
  }

  void freeze();

  bool isFrozen() const noexcept { return _frozen; }

  const AssetRecord &record(const AssetKey &) const;

  template <class T> const T &definition(const AssetId<T> &id) const {
    return std::get<T>(record(key(id)).definition);
  }

  std::vector<AssetKey> dependencies(std::span<const AssetKey> roots) const;
  std::filesystem::path resolve(const FileSource &) const;
  std::vector<std::byte> read(const AssetSource &,
                              std::size_t maximumBytes) const;
  std::string cacheKey(const AssetKey &) const;
};

struct CompiledShader {
  std::vector<std::uint32_t> words;
  rendering::ShaderReflection reflection;
  rendering::ShaderLayout layout;
};

CompiledShader prepareShader(const AssetCatalog &,
                             const AssetId<ShaderAsset> &);

} // namespace playground::assets
