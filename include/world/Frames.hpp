#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>

#include <world/World.hpp>

namespace playground::world {

struct FrameId {
  SpaceId space;
  std::uint64_t epoch{}, value{};
  auto operator<=>(const FrameId &) const = default;
};

struct FrameVersion {
  std::uint64_t epoch{}, revision{};
  bool operator==(const FrameVersion &) const = default;
};

struct ReferenceFrame {
  std::optional<FrameId> parent;
  LocalPose local;
  // Relative velocities expressed in the parent axes, or space axes for roots.
  Vec3d linear{}, angular{};
};

struct FrameSample {
  FrameId id;
  WorldVersion worldVersion;
  std::uint64_t tick{}, revision{}, discontinuity{};
  WorldPose pose;
  WorldVelocity velocity;
};

struct FrameRecord {
  FrameId id;
  ReferenceFrame definition;
  FrameSample sample;
  bool removed{};
};

struct FramePosition {
  FrameId frame;
  Vec3d offset;
};

struct FrameAttachment {
  FrameId frame;
  LocalPose local;
  Vec3d linear{}, angular{};
};

enum class FrameVelocityPolicy { PreserveWorld, FollowFrame };

WorldPosition worldPosition(FramePosition, const FrameSample &,
                            const SpatialLimits & = {});
FrameAttachment attach(const FrameSample &, WorldPose, WorldVelocity,
                       FrameVelocityPolicy, const SpatialLimits & = {});

// Resolving/detaching uses one retained frame sample for both pose and
// velocity.
struct AttachmentSample {
  WorldPose pose;
  WorldVelocity velocity;
};

AttachmentSample resolve(const FrameAttachment &, const FrameSample &,
                         const SpatialLimits & = {});

struct CreateFrame {
  FrameId id;
  ReferenceFrame props;
};

struct SetFrame {
  FrameId id;
  ReferenceFrame props;
  bool discontinuity{};
};

struct RemoveFrame {
  FrameId id;
};

using FrameMutation = std::variant<CreateFrame, SetFrame, RemoveFrame>;

struct ReferenceFramesProps {
  std::size_t maxFrames{4096}, maxCommands{4096}, maxDepth{64};
  void validate() const;
};

namespace detail {
struct FrameState;
}

class FrameSnapshot {
  std::shared_ptr<const detail::FrameState> _state;
  explicit FrameSnapshot(std::shared_ptr<const detail::FrameState>);
  friend class ReferenceFrames;

public:
  FrameVersion version() const;
  const WorldSnapshot &world() const;
  std::span<const FrameRecord> records() const;
  const FrameSample &resolve(FrameId) const;
};

class ReferenceFrames {
  ReferenceFramesProps _props;
  std::shared_ptr<rendering::ResourceLedger> _ledger;
  std::shared_ptr<const detail::FrameState> _state;

public:
  ReferenceFrames(WorldSnapshot, std::shared_ptr<rendering::ResourceLedger>,
                  ReferenceFramesProps = {});
  ReferenceFrames(const ReferenceFrames &) = delete;
  ReferenceFrames &operator=(const ReferenceFrames &) = delete;
  FrameSnapshot snapshot() const;
  // Source epoch must match; re-create the registry after world restoration.
  // Parent/child removals and reparenting validate against the entire
  // candidate.
  FrameVersion apply(WorldSnapshot, std::span<const FrameMutation>,
                     FrameVersion);
};

} // namespace playground::world
