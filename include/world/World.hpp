#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <variant>
#include <vector>

#include <runtime/ResourceLedger.hpp>
#include <world/Spatial.hpp>

namespace playground::world {
namespace detail {
struct WorldState;
}

enum class Activation { Dormant, Coarse, Full };

struct SpaceDefinition {
  SpaceId id;
  SpatialLimits limits;
};

struct EntityProps {
  WorldPose pose;
  WorldVelocity velocity;
  Activation activation{Activation::Full};
  // App-defined versioned bytes; the domain validates interpretation before
  // apply.
  std::vector<std::byte> data;
  std::optional<FrameAttachment> attachment;
};

struct EntityRecord {
  EntityId id;
  EntityProps props;
  std::uint64_t revision{}, discontinuity{};
  bool destroyed{};
};

struct EntityHandle {
  EntityId id;
  std::uint64_t epoch{};
  bool operator==(const EntityHandle &) const = default;
};

struct WorldVersion {
  std::uint64_t epoch{}, revision{};
  bool operator==(const WorldVersion &) const = default;
};

struct EntitySample {
  EntityHandle entity;
  WorldPose pose;
  WorldVelocity velocity;
  std::uint64_t tick{}, discontinuity{};
};

struct WorldProps {
  std::size_t maxSpaces{256}, maxEntities{65536}, maxCommands{4096};
  // Final logical snapshot size; temporary mutation storage is separately
  // admitted by the resource ledger, together with retained reader snapshots.
  std::size_t maxEntityBytes{1024 * 1024}, maxStateBytes{64 * 1024 * 1024};
  void validate() const;
};

// Stable checkpoint values, never runtime pointers. Restoring validates the
// entire candidate and rebinds attachment epochs before replacing live state.
struct WorldArchive {
  WorldId id;
  std::uint64_t revision{}, tick{};
  std::vector<SpaceDefinition> spaces;
  std::vector<EntityRecord> entities;
};

class WorldSnapshot {
  std::shared_ptr<const detail::WorldState> _state;
  explicit WorldSnapshot(std::shared_ptr<const detail::WorldState>);
  friend class World;

public:
  WorldId id() const noexcept;
  std::uint64_t epoch() const noexcept;
  std::uint64_t revision() const noexcept;
  std::uint64_t tick() const noexcept;
  WorldVersion version() const noexcept;
  std::span<const SpaceDefinition> spaces() const noexcept;
  // Tombstones remain represented so authored base content cannot resurrect
  // them.
  std::span<const EntityRecord> entities() const noexcept;
  const SpaceDefinition *space(SpaceId) const;
  const EntityRecord *find(EntityId) const;
  const EntityRecord &resolve(EntityHandle) const;
  EntitySample sample(EntityHandle) const;
  std::size_t reservedBytes() const noexcept;
};

struct CreateSpace {
  SpaceDefinition definition;
};

struct SpawnEntity {
  EntityId id;
  EntityProps props;
};

struct SetEntity {
  EntityId id;
  EntityProps props;
  bool discontinuity{};
};

struct DestroyEntity {
  EntityId id;
};

struct ActivationRequest {
  EntityHandle entity;
  Activation mode{Activation::Full};
  Activation authorityMinimum{Activation::Dormant};
  // Domain owner establishes required data/authority before publication.
  bool ready{};
};

using WorldMutation =
    std::variant<CreateSpace, SpawnEntity, SetEntity, DestroyEntity>;

// Owner-thread mutations; retained immutable snapshots can cross threads.
// World core owns spatial/domain bytes, not rendering, sockets or solver
// bodies.
class World {
  WorldProps _props;
  std::shared_ptr<runtime::ResourceLedger> _ledger;
  std::shared_ptr<const detail::WorldState> _state;

public:
  explicit World(WorldId, std::shared_ptr<runtime::ResourceLedger>,
                 WorldProps = {});
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  World(World &&) = delete;
  World &operator=(World &&) = delete;

  WorldSnapshot snapshot() const;
  EntityHandle handle(EntityId) const;
  // Batch/tick publication is atomic. A conflict or failed admission changes
  // nothing.
  std::uint64_t apply(std::span<const WorldMutation>, WorldVersion expected,
                      std::uint64_t tick);
  // Restore an immutable admitted snapshot into a fresh runtime incarnation.
  void restore(const WorldSnapshot &, WorldVersion expected);
  void restore(const WorldArchive &, WorldVersion expected);
  std::uint64_t setActivation(ActivationRequest, WorldVersion expected,
                              std::uint64_t tick);
};

} // namespace playground::world
