#include <chrono>
#include <limits>
#include <stdexcept>

#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/ModelPreparation.hpp>
#include <platform/sdl/TextureDecode.hpp>

namespace playground::sdl {
scene::ModelHandle prepareModel(const assets::AssetCatalog &catalog,
                                const assets::AssetId<assets::ModelAsset> &id,
                                std::stop_token stop) {
  (void)catalog.cacheKey(assets::key(id));
  const auto &definition = catalog.definition(id);
  const auto check = [&] {
    if (stop.stop_requested())
      throw std::runtime_error("Model preparation canceled");
  };
  check();
  auto document =
      catalog.read(definition.source, definition.props.maxDocumentBytes);
  scene::ModelImportServices services{
      .readResource =
          [&](std::string_view uri) {
            check();
            const auto it = definition.resources.find(std::string{uri});
            if (it == definition.resources.end())
              throw std::invalid_argument("Unregistered model dependency: " +
                                          id.value + " -> " + std::string{uri});
            return catalog.read(catalog.definition(it->second).source,
                                definition.props.maxResourceBytes);
          },
      .decodeTexture =
          [&](std::span<const std::byte> bytes, std::string_view mime,
              rendering::TextureRole role) {
            check();
            return decodeTexture(bytes, mime, role,
                                 rendering::MipPolicy::Generate,
                                 definition.props.maxResourceBytes);
          }};
  auto model = scene::importGLTF(document, services, definition.props);
  check();
  return model;
}

void ModelPreparation::checkOwner() const {
  if (std::this_thread::get_id() != _owner)
    throw std::logic_error("Model requests require their owner thread");
}

bool ModelPreparation::start(
    runtime::Executor &executor,
    std::shared_ptr<const assets::AssetCatalog> catalog,
    assets::AssetId<assets::ModelAsset> id) {
  checkOwner();
  if (!catalog || !catalog->isFrozen())
    throw std::invalid_argument("Model request requires frozen catalog");
  const auto &props = catalog->definition(id).props;
  auto definitionKey = catalog->cacheKey(assets::key(id));
  auto asset = id;
  if (_generation == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Model request generation exhausted");
  std::size_t reservation{};
  for (auto bytes : {props.maxDocumentBytes, props.maxParserBytes,
                     props.maxTotalResourceBytes}) {
    if (bytes > std::numeric_limits<std::size_t>::max() - reservation)
      throw std::overflow_error("Model reservation overflow");
    reservation += bytes;
  }
  auto promise = std::make_shared<std::promise<scene::ModelHandle>>();
  auto future = promise->get_future();
  auto ticket = executor.submit(
      [catalog = std::move(catalog), id = std::move(id),
       promise](std::stop_token stop) noexcept {
        try {
          promise->set_value(prepareModel(*catalog, id, stop));
        } catch (...) {
          promise->set_exception(std::current_exception());
        }
      },
      reservation);
  if (!ticket)
    return false;
  cancel();
  ++_generation;
  _ticket = std::move(ticket);
  _future = std::move(future);
  _asset = std::move(asset);
  _definitionKey = std::move(definitionKey);
  return true;
}

void ModelPreparation::cancel() noexcept {
  if (_ticket)
    _ticket->cancel();
  _ticket.reset();
  _future = {};
}

bool ModelPreparation::isPending() const {
  checkOwner();
  return _future.valid();
}

std::optional<ModelPreparationResult> ModelPreparation::poll() {
  checkOwner();
  if (!_future.valid() ||
      _future.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
    return {};
  ModelPreparationResult result{.generation = _generation,
                                .asset = std::move(_asset),
                                .definitionKey = std::move(_definitionKey)};
  try {
    result.model = _future.get();
  } catch (...) {
    result.error = std::current_exception();
  }
  _ticket.reset();
  return result;
}
} // namespace playground::sdl
