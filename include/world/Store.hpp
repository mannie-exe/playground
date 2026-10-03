#pragma once

#include <functional>
#include <map>
#include <stdexcept>
#include <string>

#include <world/Frames.hpp>

namespace playground::world {

struct StoredCheckpoint {
  std::uint64_t revision{};
  std::vector<std::byte> bytes;
};

// Publication may already be visible but durable acknowledgement failed.
// Re-read/reconcile; never blindly retry with the old store revision.
class CheckpointUncertain : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

// Implementations serialize compare-and-swap across all writers of one world.
// Successful publication acknowledges their documented durable flush boundary.
class CheckpointStorage {
public:
  virtual ~CheckpointStorage() = default;
  virtual std::optional<StoredCheckpoint> read(WorldId,
                                               std::size_t maxBytes) = 0;
  virtual std::uint64_t publish(WorldId, std::uint64_t expectedRevision,
                                std::span<const std::byte>,
                                std::size_t maxBytes) = 0;
};

struct PersistentRecord {
  std::string key;
  std::uint64_t schema{1};
  // Opaque domain state, exact unloaded-cell references, generator identity or
  // edits. Retained unchanged when that domain/provider is unavailable.
  std::vector<std::byte> bytes;
};

struct CheckpointMetadata {
  std::string contentLock;
  std::vector<PersistentRecord> records;
};

struct WorldStoreProps {
  std::size_t maxCheckpointBytes{128 * 1024 * 1024};
  std::size_t maxRecords{4096}, maxFrames{4096};
  WorldProps world;
  void validate() const;
};

struct WorldSaveResult {
  std::uint64_t storeRevision{};
  WorldVersion captured;
  std::uint64_t tick{};
};

struct RestoredWorld {
  runtime::ResourceLedger::Token charge;
  std::unique_ptr<World> world;
  std::unique_ptr<ReferenceFrames> frames;
  CheckpointMetadata metadata;
  std::uint64_t storeRevision{};
};

class WorldStore {
  std::shared_ptr<CheckpointStorage> _storage;
  std::shared_ptr<runtime::ResourceLedger> _ledger;
  WorldStoreProps _props;
  // Migration returns one complete current-format candidate; bounded and
  // validated again before any active world replacement.
  std::map<std::uint64_t, std::function<std::vector<std::byte>(
                              std::span<const std::byte>, std::size_t)>>
      _migrations;

public:
  static constexpr std::uint64_t schemaVersion = 1;
  WorldStore(std::shared_ptr<CheckpointStorage>,
             std::shared_ptr<runtime::ResourceLedger>, WorldStoreProps = {});
  void
  addMigration(std::uint64_t oldSchema,
               std::function<std::vector<std::byte>(std::span<const std::byte>,
                                                    std::size_t)>);
  WorldSaveResult save(const WorldSnapshot &, const FrameSnapshot &,
                       const CheckpointMetadata &,
                       std::uint64_t expectedStoreRevision);
  // Missing is nullptr. Decode/migration failure throws without replacing the
  // caller's live world. Swap this complete bundle at its owner boundary.
  std::unique_ptr<RestoredWorld> load(WorldId);
};
} // namespace playground::world
