#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include <assets/AssetCatalog.hpp>

namespace playground::assets {
namespace {
std::atomic<std::uint64_t> nextIdentity{1};

std::string pathText(const std::filesystem::path &path) {
  const auto utf8 = path.generic_u8string();
  return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

void validateName(const std::string &name) {
  if (name.empty() || name.size() > 256 ||
      !std::ranges::all_of(name, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
      }))
    throw std::invalid_argument("Invalid logical asset identity: " + name);
}

void validatePath(const std::filesystem::path &path) {
  const auto text = pathText(path);
  if (path.empty() || path.has_root_path() ||
      text.find_first_of(":\\") != std::string::npos ||
      text.find('\0') != std::string::npos)
    throw std::invalid_argument(
        "Asset path must be relative to its content root");
  for (const auto &part : path)
    if (part == "..")
      throw std::invalid_argument("Asset path escapes content root");
}
} // namespace

AssetCatalog::AssetCatalog(std::filesystem::path root)
    : _identity{nextIdentity.fetch_add(1)},
      _root{
          std::filesystem::weakly_canonical(std::filesystem::absolute(root))} {}

void AssetCatalog::addRecord(AssetKey id, AssetRecord record) {
  if (_frozen)
    throw std::logic_error("Cannot modify a frozen asset catalog");
  validateName(id.value);
  if (!record.revision || id.type != record.definition.index())
    throw std::invalid_argument("Invalid asset revision/type");
  std::visit(
      [&](const auto &definition) {
        using T = std::decay_t<decltype(definition)>;
        if constexpr (std::is_same_v<T, MeshAsset>) {
          if (!definition.mesh)
            throw std::invalid_argument(
                "Mesh asset requires immutable geometry");
        } else if constexpr (std::is_same_v<T, FontAsset>) {
          validatePath(definition.source.path);
        } else {
          if (const auto *file = std::get_if<FileSource>(&definition.source))
            validatePath(file->path);
        }
        if constexpr (std::is_same_v<T, ModelAsset>) {
          definition.props.validate();
          for (const auto &[uri, resource] : definition.resources) {
            if (uri.empty() || uri.find('\0') != std::string::npos)
              throw std::invalid_argument("Invalid model resource URI");
            record.dependencies.push_back(key(resource));
          }
        }
        if constexpr (std::is_same_v<T, ShaderAsset>) {
          if (definition.stage != rendering::ShaderStage::Vertex &&
              definition.stage != rendering::ShaderStage::Fragment)
            throw std::invalid_argument("Invalid shader stage");
          if (definition.entryPoint.empty() ||
              definition.entryPoint.find('\0') != std::string::npos)
            throw std::invalid_argument("Shader requires an entry point");
        }
      },
      record.definition);
  for (const auto &dependency : record.dependencies)
    validateName(dependency.value);
  if (!_records.emplace(std::move(id), std::move(record)).second)
    throw std::invalid_argument("Duplicate asset identity/type");
}

const AssetRecord &AssetCatalog::record(const AssetKey &id) const {
  const auto it = _records.find(id);
  if (it == _records.end())
    throw std::out_of_range("Unregistered asset: " + id.value);
  return it->second;
}

std::vector<AssetKey>
AssetCatalog::dependencies(std::span<const AssetKey> roots) const {
  // Iterative DFS bounds call-stack use even for deeply authored graphs.
  std::map<AssetKey, unsigned> state;
  std::vector<AssetKey> output;

  struct Frame {
    AssetKey id;
    std::size_t next{};
  };

  for (const auto &root : roots) {
    if (state[root] == 2)
      continue;
    std::vector<Frame> stack{{root}};
    state[root] = 1;
    while (!stack.empty()) {
      auto &frame = stack.back();
      const auto &edges = record(frame.id).dependencies;
      if (frame.next == edges.size()) {
        state[frame.id] = 2;
        output.push_back(frame.id);
        stack.pop_back();
      } else {
        const auto &child = edges[frame.next++];
        if (state[child] == 1)
          throw std::invalid_argument("Asset dependency cycle: " +
                                      frame.id.value + " -> " + child.value);
        if (state[child] == 0) {
          if (!_records.contains(child))
            throw std::invalid_argument("Missing asset dependency: " +
                                        frame.id.value + " -> " + child.value);
          state[child] = 1;
          stack.push_back({child});
        }
      }
    }
  }
  return output;
}

void AssetCatalog::freeze() {
  if (_frozen)
    return;
  std::vector<AssetKey> roots;
  for (const auto &[id, entry] : _records)
    roots.push_back(id);
  (void)dependencies(roots);
  _frozen = true;
}

std::filesystem::path AssetCatalog::resolve(const FileSource &source) const {
  validatePath(source.path);
  const auto result = std::filesystem::weakly_canonical(_root / source.path);
  const auto relative = result.lexically_relative(_root);
  if (relative.empty() || relative.has_root_path())
    throw std::invalid_argument("Asset is outside content root");
  for (const auto &part : relative)
    if (part == "..")
      throw std::invalid_argument("Asset symlink escapes content root");
  return result;
}

std::vector<std::byte> AssetCatalog::read(const AssetSource &source,
                                          std::size_t limit) const {
  if (const auto *owned = std::get_if<ByteSource>(&source)) {
    if (owned->bytes.size() > limit)
      throw std::length_error("Asset exceeds byte budget");
    return owned->bytes;
  }
  const auto path = resolve(std::get<FileSource>(source));
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file)
    throw std::runtime_error("Cannot open asset: " + pathText(path));
  const auto length = file.tellg();
  if (length < 0 || static_cast<std::uintmax_t>(length) > limit ||
      static_cast<std::uintmax_t>(length) >
          static_cast<std::uintmax_t>(
              std::numeric_limits<std::streamsize>::max()))
    throw std::length_error("Asset exceeds byte budget: " + pathText(path));
  std::vector<std::byte> bytes(static_cast<std::size_t>(length));
  file.seekg(0);
  if (!file.read(reinterpret_cast<char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size())))
    throw std::runtime_error("Cannot read asset: " + pathText(path));
  return bytes;
}

std::string AssetCatalog::cacheKey(const AssetKey &id) const {
  if (!_frozen)
    throw std::logic_error("Freeze assets before resource acquisition");
  const auto &entry = record(id);
  return std::to_string(_identity) + ":" + std::to_string(id.type) + ":" +
         id.value + ":" + std::to_string(entry.revision);
}

CompiledShader prepareShader(const AssetCatalog &catalog,
                             const AssetId<ShaderAsset> &id) {
  (void)catalog.cacheKey(key(id));
  const auto &definition = catalog.definition(id);
  const auto bytes = catalog.read(definition.source, 16 * 1024 * 1024);
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t))
    throw std::invalid_argument("Invalid SPIR-V byte length: " + id.value);
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  auto reflection = rendering::reflectSPIRV(words, definition.entryPoint);
  if (reflection.stage != definition.stage)
    throw std::invalid_argument("Shader stage mismatch: " + id.value);
  reflection.validateLayout(definition.layout);
  return {std::move(words), std::move(reflection), definition.layout};
}
} // namespace playground::assets
