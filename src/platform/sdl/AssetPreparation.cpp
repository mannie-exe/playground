#include <chrono>
#include <limits>
#include <stdexcept>

#include <platform/sdl/AssetPreparation.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <platform/sdl/ModelImport.hpp>
#include <platform/sdl/TextureDecode.hpp>

namespace playground::sdl {
scene::ModelHandle
prepareModel(const assets::AssetCatalog &catalog,
             const assets::AssetId<assets::ModelAsset> &id,
             std::stop_token stop,
             std::shared_ptr<runtime::ResourceLedger> ledger) {
  PreparationBudget budget{ledger};
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
      .resources = ledger,
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
                                 definition.props.maxResourceBytes, budget);
          }};
  auto model = scene::importGLTF(document, services, definition.props);
  // Prepare color upload variants once on the model worker, not each frame.
  for (const auto &node : model->nodes())
    for (const auto &primitive : node.primitives) {
      check();
      const auto &material = primitive.material;
      const auto &color =
          material.pbr ? material.pbr->baseColorTexture : material.colorTexture;
      if (color.texture)
        color.texture->upload(material.alpha ==
                              scene::MaterialProps::Alpha::Opaque);
    }
  check();
  return model;
}

void AssetPreparation::checkOwner() const {
  if (std::this_thread::get_id() != _owner)
    throw std::logic_error("Asset requests require their owner thread");
}

runtime::TaskAdmission AssetPreparation::start(runtime::Executor &executor,
                                               AssetResources &resources,
                                               AssetPreparationProps props,
                                               std::function<void()> wake) {
  checkOwner();
  if (_generation == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Asset request generation exhausted");
  if (props.models.size() > 256 || props.textures.size() > 256)
    throw std::length_error("Asset request exceeds batch limit");
  auto prepared = resources.cached(props);
  auto catalog = resources.catalogHandle();
  auto ledger = resources.cache().resources();
  std::size_t reservation{};
  bool missing{};
  for (const auto &model : prepared.models) {
    if (model.model)
      continue;
    missing = true;
    const auto &p = catalog->definition(model.asset).props;
    if (p.maxParserBytes >
        std::numeric_limits<std::size_t>::max() - p.maxPreparationBytes)
      throw std::overflow_error("Model scratch reservation overflow");
    reservation =
        std::max(reservation, p.maxParserBytes + p.maxPreparationBytes);
  }
  for (std::size_t i = 0; i < prepared.textures.size(); ++i)
    if (!prepared.textures[i].second) {
      missing = true;
      reservation = std::max(reservation, props.textures[i].maximumBytes);
    }
  if (props.environment && !prepared.environment) {
    missing = true;
    reservation = std::max(reservation, props.environment->maximumBytes);
  }
  auto promise = std::make_shared<std::promise<PreparedAssets>>();
  auto future = promise->get_future();
  std::optional<runtime::TaskTicket> ticket;
  if (missing) {
    auto submission = executor.submit(
        [catalog, ledger, props = std::move(props),
         prepared = std::move(prepared), promise,
         wake = std::move(wake)](std::stop_token stop) mutable noexcept {
          try {
            PreparationBudget budget{ledger};
            const auto check = [&] {
              if (stop.stop_requested())
                throw std::runtime_error("Asset preparation canceled");
            };
            for (std::size_t i = 0; i < prepared.models.size(); ++i) {
              check();
              auto &model = prepared.models[i];
              if (model.model)
                continue;
              for (std::size_t j = 0; j < i; ++j)
                if (prepared.models[j].definitionKey == model.definitionKey)
                  model.model = prepared.models[j].model;
              if (!model.model) {
                try {
                  model.model =
                      prepareModel(*catalog, model.asset, stop, ledger);
                } catch (const runtime::ResourcePressure &) {
                  throw;
                } catch (const std::exception &e) {
                  throw std::runtime_error(model.asset.value + ": " + e.what());
                }
              }
            }
            for (std::size_t i = 0; i < prepared.textures.size(); ++i) {
              check();
              auto &[key, texture] = prepared.textures[i];
              if (texture)
                continue;
              for (std::size_t j = 0; j < i; ++j)
                if (prepared.textures[j].first == key)
                  texture = prepared.textures[j].second;
              if (!texture) {
                const auto &request = props.textures[i];
                const auto bytes =
                    catalog->read(catalog->definition(request.asset).source,
                                  request.maximumBytes);
                texture = decodeTexture(bytes, {}, request.role, request.mip,
                                        request.maximumBytes, budget);
              }
            }
            if (props.environment && !prepared.environment) {
              check();
              const auto &request = *props.environment;
              const auto bytes =
                  catalog->read(catalog->definition(request.asset).source,
                                request.maximumBytes);
              auto source = decodeHDR(bytes, request.maximumBytes, budget);
              auto lighting =
                  scene::prepareEnvironment(*source, request.props, stop);
              prepared.environment =
                  std::make_shared<const EnvironmentResource>(
                      EnvironmentResource{std::move(source),
                                          std::move(lighting)});
            }
            check();
            promise->set_value(std::move(prepared));
          } catch (...) {
            promise->set_exception(std::current_exception());
          }
          if (wake)
            try {
              wake();
            } catch (...) {
            }
        },
        reservation);
    if (!submission.ticket)
      return submission.admission;
    ticket = std::move(submission.ticket);
  } else {
    promise->set_value(std::move(prepared));
  }
  cancel();
  ++_generation;
  _ticket = std::move(ticket);
  _future = std::move(future);
  if (!missing && wake)
    try {
      wake();
    } catch (...) {
    }
  return runtime::TaskAdmission::Accepted;
}

void AssetPreparation::cancel() noexcept {
  if (_ticket)
    _ticket->cancel();
  _ticket.reset();
  _future = {};
}

bool AssetPreparation::isPending() const {
  checkOwner();
  return _future.valid();
}

bool AssetPreparation::isReady() const {
  checkOwner();
  return _future.valid() &&
         _future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

std::optional<AssetPreparationResult> AssetPreparation::poll() {
  if (!isReady())
    return {};
  AssetPreparationResult result{.generation = _generation};
  try {
    result.assets = _future.get();
  } catch (...) {
    result.error = std::current_exception();
  }
  _ticket.reset();
  return result;
}
} // namespace playground::sdl
