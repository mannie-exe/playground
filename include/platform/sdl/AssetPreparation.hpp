#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <assets/AssetCatalog.hpp>
#include <runtime/Executor.hpp>
#include <scene/Environment.hpp>

namespace playground::sdl {

// Synchronous CPU-only import. Reads only registered sources; PNG/JPEG decoding
// uses private streams/surfaces, never fonts, cache mutation or native GPU
// work.
scene::ModelHandle prepareModel(const assets::AssetCatalog &,
                                const assets::AssetId<assets::ModelAsset> &,
                                std::stop_token stop = {},
                                std::shared_ptr<runtime::ResourceLedger>
                                    ledger = runtime::defaultResourceLedger());

struct PreparedModel {
  assets::AssetId<assets::ModelAsset> asset;
  std::string definitionKey;
  scene::ModelHandle model;
};

struct TextureRequest {
  assets::AssetId<assets::BinaryAsset> asset;
  rendering::TextureRole role{rendering::TextureRole::Color};
  rendering::MipPolicy mip{rendering::MipPolicy::Generate};
  std::size_t maximumBytes{256 * 1024 * 1024};
};

struct EnvironmentRequest {
  assets::AssetId<assets::BinaryAsset> asset;
  scene::EnvironmentProps props;
  std::size_t maximumBytes{256 * 1024 * 1024};
};

struct EnvironmentResource {
  rendering::TextureHandle source;
  scene::PreparedEnvironment lighting;
};

struct AssetPreparationProps {
  std::vector<assets::AssetId<assets::ModelAsset>> models;
  std::vector<TextureRequest> textures;
  std::optional<EnvironmentRequest> environment;
};

struct PreparedAssets {
  std::vector<PreparedModel> models;
  std::vector<std::pair<std::string, rendering::TextureHandle>> textures;
  std::string environmentKey;
  std::shared_ptr<const EnvironmentResource> environment;
};

struct AssetPreparationResult {
  std::uint64_t generation{};
  PreparedAssets assets;
  std::exception_ptr error;
};

class AssetResources;

// Owner-thread request slot; sequential worker preparation bounds concurrent
// scratch. Rejected admission preserves the previous request. Cache publication
// is explicit, owner-thread-only, and validates catalog/recipe identities.
class AssetPreparation {
  const std::thread::id _owner{std::this_thread::get_id()};
  std::uint64_t _generation{};
  std::optional<runtime::TaskTicket> _ticket;
  std::future<PreparedAssets> _future;
  void checkOwner() const;

public:
  ~AssetPreparation() { cancel(); }

  AssetPreparation() = default;
  AssetPreparation(const AssetPreparation &) = delete;
  AssetPreparation &operator=(const AssetPreparation &) = delete;
  runtime::TaskAdmission start(runtime::Executor &, AssetResources &,
                               AssetPreparationProps,
                               std::function<void()> wake = {});
  void cancel() noexcept;
  bool isPending() const;
  bool isReady() const;
  std::optional<AssetPreparationResult> poll();
};
} // namespace playground::sdl
