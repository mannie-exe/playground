#pragma once

#include <map>
#include <optional>
#include <span>
#include <vector>

#include <world/Frames.hpp>
#include <world/Streaming.hpp>

namespace playground::world {

struct ActivationPolicy {
  double coarseSeconds{1};
  void validate() const;
};

struct ActivationStep {
  EntityHandle entity;
  Activation mode;
  double seconds{};
  bool wake{};
};

struct ActivationBatch {
  runtime::ResourceLedger::Token charge;
  std::vector<ActivationStep> steps;
  ActivationBatch() = default;
  ActivationBatch(ActivationBatch &&) = default;
  ActivationBatch &operator=(ActivationBatch &&) = default;
};

// Simulation time only. A changed mode/discontinuity starts a new elapsed-time
// interval; dormant time never becomes a catch-up integration step.
class ActivationScheduler {
  struct Entry {
    Activation mode{Activation::Dormant};
    double last{};
    std::uint64_t discontinuity{};
    std::optional<double> wakeAt;
  };

  runtime::ResourceLedger::Token _charge;
  std::shared_ptr<runtime::ResourceLedger> _ledger;
  WorldId _world;
  std::uint64_t _epoch;
  std::uint64_t _revision{}, _tick{};
  ActivationPolicy _policy;
  std::size_t _maximum;
  std::map<EntityId, Entry> _entries;
  double _now{};

public:
  ActivationScheduler(const WorldSnapshot &,
                      std::shared_ptr<runtime::ResourceLedger>,
                      ActivationPolicy = {}, std::size_t maxEntities = 65536);
  void wake(EntityHandle, double simulationSeconds);
  ActivationBatch advance(const WorldSnapshot &, double simulationSeconds);
};

struct ZoneId {
  WorldId world;
  std::uint64_t value{};
  auto operator<=>(const ZoneId &) const = default;
};

struct ZoneDefinition {
  ZoneId id;
  WorldBounds bounds; // Closed boundary: touching is inside.
};

struct ZoneObservation {
  EntityId entity;
  std::vector<ZoneId> inside;
  bool complete{true};
};

enum class ZoneEventReason { Initial, Movement, Teleport, Destroyed };

struct ZoneEvent {
  ZoneId zone;
  EntityId entity;
  bool entered{};
  ZoneEventReason reason;
};

struct ZoneEvents {
  runtime::ResourceLedger::Token charge;
  WorldVersion version;
  std::vector<ZoneEvent> values;
  ZoneEvents() = default;
  ZoneEvents(ZoneEvents &&) = default;
  ZoneEvents &operator=(ZoneEvents &&) = default;
};

// Observations can come from bounds or a query provider with explicit coverage.
// Incomplete/absent observations never prove exits; tombstones do.
class ZoneTracker {
  struct Membership {
    std::vector<ZoneId> zones;
    std::uint64_t discontinuity{};
  };

  runtime::ResourceLedger::Token _charge;
  std::shared_ptr<runtime::ResourceLedger> _ledger;
  WorldVersion _version;
  WorldId _world;
  std::size_t _maximum;
  std::map<ZoneId, ZoneDefinition> _zones;
  std::map<EntityId, Membership> _members;

public:
  ZoneTracker(const WorldSnapshot &, std::span<const ZoneDefinition>,
              std::shared_ptr<runtime::ResourceLedger>,
              std::size_t maxEntities = 4096);
  ZoneEvents update(const WorldSnapshot &, std::span<const ZoneObservation>);
  ZoneEvents updateBounds(const WorldSnapshot &);
};

enum class TransferVelocity { Reset, MapRigid };
enum class TransferStatus {
  Preparing,
  Ready,
  Committed,
  Cancelled,
  Failed,
  Unsupported
};

struct TransferEntity {
  EntityHandle entity;
  WorldPose destination;
  TransferVelocity velocity{TransferVelocity::Reset};
  math::Quaternion velocityRotation{};
  // Omission explicitly detaches. A supplied sample must match the current
  // frame registry and source world tick at prepare and commit.
  std::optional<FrameSample> destinationFrame;
};

struct TransferRequest {
  std::vector<TransferEntity> entities; // Entire declared dependency group.
  std::vector<CellRequest> readiness;
  bool authorized{}, requirePhysicalPlacement{};
  std::optional<runtime::ActivityClock::time_point> deadline;
};

struct TransferResult {
  TransferStatus status{TransferStatus::Preparing};
  std::optional<WorldVersion> committed;
  std::string diagnostic;
};

// Prepared local transfers mutate a single world/authority. Physical placement
// is explicitly unsupported until a motor/query adapter implements it.
class WorldTransfer {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  WorldTransfer(World &, WorldStreamer *,
                std::shared_ptr<runtime::ResourceLedger>,
                std::size_t maxTickets = 64, std::size_t maxEntities = 256,
                const ReferenceFrames * = nullptr);
  ~WorldTransfer();
  std::uint64_t prepare(TransferRequest);
  TransferResult state(std::uint64_t) const;
  void advance(runtime::ActivityClock::time_point);
  WorldVersion commit(std::uint64_t, std::uint64_t tick,
                      runtime::ActivityClock::time_point);
  void cancel(std::uint64_t);
  void forget(std::uint64_t);
  void close() noexcept;
};
} // namespace playground::world
