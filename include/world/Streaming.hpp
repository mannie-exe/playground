#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include <runtime/Executor.hpp>
#include <runtime/Services.hpp>
#include <world/Queries.hpp>

namespace playground::world {

struct CellId {
  SpaceId space;
  std::uint64_t value{};
  auto operator<=>(const CellId &) const = default;
};

enum class Readiness : unsigned {
  None = 0,
  Data = 1,
  Query = 2,
  Navigation = 4,
  Simulation = 8,
  Presentation = 16
};

constexpr Readiness operator|(Readiness a, Readiness b) {
  return Readiness(unsigned(a) | unsigned(b));
}

constexpr bool satisfies(Readiness available, Readiness required) {
  return (unsigned(available) & unsigned(required)) == unsigned(required);
}

struct CellDefinition {
  CellId id;
  WorldBounds bounds;
  std::uint64_t revision{1};
  std::string
      content; // Exact provider content key, never an implicit URL fetch.
  std::vector<CellId> dependencies;
};

struct WorldManifest {
  WorldId world;
  std::uint64_t revision{1};
  std::string contentLock;
  std::vector<CellDefinition> cells;
  std::vector<SpaceDefinition> spaces;
};

struct StreamingSource {
  WorldPosition position;
  Vec3d velocity;
  double loadRadius{}, unloadRadius{}, predictionSeconds{};
  int priority{};
  Readiness required{Readiness::Data};
};

struct CellRequest {
  CellId cell;
  std::uint64_t epoch{}, revision{}, generation{};
  Readiness required{Readiness::Data};
  int priority{};
  std::optional<runtime::ActivityClock::time_point> deadline;
};

// Providers own immutable typed products behind this base. A readiness bit
// promises that the corresponding product really exists at this revision.
// Encoded package/procedural sources require an explicitly supplied provider.
class CellProduct {
public:
  virtual ~CellProduct() = default;
  virtual Readiness readiness() const noexcept = 0;
  virtual std::size_t bytes() const noexcept = 0;
};

class CellProvider {
public:
  virtual ~CellProvider() = default;
  virtual Readiness capabilities() const noexcept = 0;
  // Worker-only, may run concurrently; no live world mutation. Budget includes
  // all product storage; provider scratch must fit scratchBytes. Stop is
  // cooperative.
  virtual std::shared_ptr<const CellProduct>
  prepare(const CellDefinition &, Readiness, std::size_t productBytes,
          std::size_t scratchBytes,
          std::span<const std::shared_ptr<const CellProduct>> dependencies,
          std::stop_token) = 0;
};

enum class CellStatus { Absent, Queued, Preparing, Ready, Failed, Cancelled };

struct CellState {
  CellStatus status{CellStatus::Absent};
  Readiness requested{Readiness::None}, available{Readiness::None};
  std::uint64_t revision{}, generation{};
  bool stale{};
  std::size_t bytes{}, leases{};
  std::string diagnostic;
};

namespace detail {
struct CellGeneration;
}

class CellLease {
  std::shared_ptr<const detail::CellGeneration> _generation;
  explicit CellLease(std::shared_ptr<const detail::CellGeneration>);
  friend class WorldStreamer;

public:
  CellLease() = default;

  explicit operator bool() const noexcept { return bool(_generation); }

  const CellProduct &product() const;
  CellId cell() const;
  std::uint64_t revision() const;
};

struct StreamingProps {
  std::size_t maxCells{4096}, maxSources{64}, maxRequests{256}, maxJobs{4};
  std::size_t maxRetiring{4096};
  std::size_t productBytes{8 * 1024 * 1024}, scratchBytes{8 * 1024 * 1024};
  double minimumRetentionSeconds{1}, agingSeconds{1},
      maximumPredictionSeconds{2};
  void validate() const;
};

struct StreamingStats {
  std::size_t queued{}, preparing{}, ready{}, stale{}, leased{},
      retiringBytes{};
  std::uint64_t published{}, cancellations{}, failures{}, deadlineMisses{};
  double oldestQueueSeconds{}, preparationSeconds{}, publicationSeconds{};
};

// Owner-thread control; worker results live in independent retained slots.
// Closing cancels jobs without waiting; the executor/ledger outlive their work.
class WorldStreamer {
  struct Impl;
  std::shared_ptr<Impl> _impl;
  static runtime::ServiceWork advanceImpl(Impl &,
                                          runtime::ActivityClock::time_point,
                                          runtime::ServiceWorkBudget);
  static runtime::ServiceDemand demandImpl(const Impl &);

public:
  WorldStreamer(WorldManifest, std::uint64_t epoch,
                std::shared_ptr<CellProvider>, runtime::Executor &,
                std::shared_ptr<runtime::ResourceLedger>, StreamingProps = {},
                std::function<void()> wake = {});
  ~WorldStreamer();
  WorldStreamer(const WorldStreamer &) = delete;
  WorldStreamer &operator=(const WorldStreamer &) = delete;
  void setSources(std::uint64_t owner, std::span<const StreamingSource>);
  std::uint64_t request(CellRequest);
  void cancel(std::uint64_t request);
  void forget(std::uint64_t request);
  CellState requestState(std::uint64_t request) const;
  CellState state(CellId) const;
  CellLease lease(CellId, Readiness, std::uint64_t revision) const;
  void replace(CellDefinition);
  runtime::ServiceWork advance(runtime::ActivityClock::time_point,
                               runtime::ServiceWorkBudget = {});
  runtime::ServiceDemand demand() const;
  runtime::ServiceHandle attach(runtime::ServiceScope &);
  StreamingStats stats() const;
  void close() noexcept;
};
} // namespace playground::world
