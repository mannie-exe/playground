#pragma once

#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <thread>

#include <assets/AssetCatalog.hpp>
#include <runtime/Executor.hpp>

namespace playground::sdl {

// Synchronous CPU-only import. Reads only registered sources; PNG/JPEG decoding
// uses private streams/surfaces, never fonts, cache mutation or native GPU
// work.
scene::ModelHandle prepareModel(const assets::AssetCatalog &,
                                const assets::AssetId<assets::ModelAsset> &,
                                std::stop_token stop = {},
                                std::shared_ptr<runtime::ResourceLedger>
                                    ledger = runtime::defaultResourceLedger());

struct ModelPreparationResult {
  std::uint64_t generation;
  assets::AssetId<assets::ModelAsset> asset;
  std::string definitionKey;
  scene::ModelHandle model;
  std::exception_ptr error;
};

// Per-consumer latest-request slot. Owner polls without blocking; accepted new
// requests supersede old work. Rejected admission preserves the old request.
class ModelPreparation {
  const std::thread::id _owner{std::this_thread::get_id()};
  std::uint64_t _generation{};
  std::optional<runtime::TaskTicket> _ticket;
  std::future<scene::ModelHandle> _future;
  assets::AssetId<assets::ModelAsset> _asset;
  std::string _definitionKey;

  void checkOwner() const;

public:
  ~ModelPreparation() { cancel(); }

  ModelPreparation() = default;
  ModelPreparation(const ModelPreparation &) = delete;
  ModelPreparation &operator=(const ModelPreparation &) = delete;
  bool start(runtime::Executor &, std::shared_ptr<const assets::AssetCatalog>,
             assets::AssetId<assets::ModelAsset>,
             std::shared_ptr<runtime::ResourceLedger> ledger =
                 runtime::defaultResourceLedger());
  void cancel() noexcept;
  bool isPending() const;
  std::optional<ModelPreparationResult> poll();
};
} // namespace playground::sdl
