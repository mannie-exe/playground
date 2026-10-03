#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <world/Frames.hpp>

namespace playground::world {
namespace detail {
struct FrameState {
  runtime::ResourceLedger::Token charge;
  WorldSnapshot world;
  std::uint64_t revision{};
  std::vector<FrameRecord> records;

  explicit FrameState(WorldSnapshot source) : world{std::move(source)} {}
};
} // namespace detail

namespace {
std::uint64_t increment(std::uint64_t value) {
  if (value == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Frame revision exhausted");
  return value + 1;
}

void valid(FrameId id, const WorldSnapshot &world) {
  if (!id.value || id.epoch != world.epoch() || !world.space(id.space))
    throw std::invalid_argument("Frame identity is outside world epoch/space");
}

void valid(const FrameSample &sample, const SpatialLimits &limits) {
  if (!sample.id.value || !sample.id.epoch ||
      sample.id.epoch != sample.worldVersion.epoch || !sample.revision ||
      !sample.discontinuity || sample.id.space != sample.pose.position.space ||
      sample.id.space != sample.velocity.space)
    throw std::invalid_argument("Invalid frame sample identity");
  validate(sample.pose, limits);
  validate(sample.velocity);
}

void valid(FrameAttachment attachment, const FrameSample &frame,
           const SpatialLimits &limits) {
  valid(frame, limits);
  if (attachment.frame != frame.id || !isFinite(attachment.local.offset) ||
      !isFinite(attachment.linear) || !isFinite(attachment.angular))
    throw std::invalid_argument("Attachment does not match frame sample");
}

template <class Records> auto lookup(Records &records, FrameId id) {
  return std::lower_bound(
      records.begin(), records.end(), id,
      [](const auto &record, FrameId key) { return record.id < key; });
}

std::size_t estimate(std::size_t count) {
  constexpr auto perFrame = sizeof(FrameRecord) + sizeof(std::size_t) + 2;
  if (count >
      (std::numeric_limits<std::size_t>::max() - sizeof(detail::FrameState)) /
          perFrame)
    throw std::length_error("Frame storage estimate overflow");
  return sizeof(detail::FrameState) + count * perFrame;
}
} // namespace

WorldPosition worldPosition(FramePosition position, const FrameSample &sample,
                            const SpatialLimits &limits) {
  valid(sample, limits);
  if (position.frame != sample.id)
    throw std::invalid_argument("Frame position uses a different frame");
  return compose(sample.pose, {position.offset, {}}, limits).position;
}

FrameAttachment attach(const FrameSample &sample, WorldPose pose,
                       WorldVelocity velocity, FrameVelocityPolicy policy,
                       const SpatialLimits &limits) {
  valid(sample, limits);
  validate(pose, limits);
  validate(velocity);
  if (pose.position.space != sample.id.space ||
      velocity.space != sample.id.space)
    throw std::invalid_argument(
        "Attachment requires same-space pose and velocity");
  FrameAttachment result{sample.id, relativePose(pose, sample.pose, limits)};
  switch (policy) {
  case FrameVelocityPolicy::PreserveWorld: {
    const auto base =
        attachedVelocity(sample.pose, sample.velocity, result.local.offset);
    result.linear =
        inverseRotate(sample.pose.orientation, velocity.linear - base.linear);
    result.angular = inverseRotate(sample.pose.orientation,
                                   velocity.angular - sample.velocity.angular);
    break;
  }
  case FrameVelocityPolicy::FollowFrame:
    break;
  default:
    throw std::invalid_argument("Unknown attachment velocity policy");
  }
  return result;
}

AttachmentSample resolve(const FrameAttachment &attachment,
                         const FrameSample &sample,
                         const SpatialLimits &limits) {
  valid(attachment, sample, limits);
  auto velocity = attachedVelocity(sample.pose, sample.velocity,
                                   attachment.local.offset, attachment.linear);
  velocity.angular = sample.velocity.angular +
                     rotate(sample.pose.orientation, attachment.angular);
  return {compose(sample.pose, attachment.local, limits), velocity};
}

void ReferenceFramesProps::validate() const {
  if (!maxFrames || !maxCommands || !maxDepth || maxDepth > 256)
    throw std::invalid_argument(
        "Reference frame limits must be positive with depth at most 256");
  estimate(maxFrames);
}

FrameSnapshot::FrameSnapshot(std::shared_ptr<const detail::FrameState> state)
    : _state{std::move(state)} {}

FrameVersion FrameSnapshot::version() const {
  return {_state->world.epoch(), _state->revision};
}

const WorldSnapshot &FrameSnapshot::world() const { return _state->world; }

std::span<const FrameRecord> FrameSnapshot::records() const {
  return _state->records;
}

const FrameSample &FrameSnapshot::resolve(FrameId id) const {
  valid(id, _state->world);
  const auto it = lookup(_state->records, id);
  if (it == _state->records.end() || it->id != id || it->removed)
    throw std::invalid_argument("Frame is missing or removed");
  return it->sample;
}

ReferenceFrames::ReferenceFrames(
    WorldSnapshot world, std::shared_ptr<runtime::ResourceLedger> ledger,
    ReferenceFramesProps props)
    : _props{props}, _ledger{std::move(ledger)} {
  props.validate();
  if (!_ledger)
    throw std::invalid_argument("Frame accounting required");
  auto charge = _ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::World, estimate(0),
      "Frame snapshot", {world.id().value, world.epoch(), 0});
  auto state = std::make_shared<detail::FrameState>(std::move(world));
  state->charge = std::move(charge);
  state->charge->setState(runtime::AllocationState::Owned);
  _state = std::move(state);
}

FrameSnapshot ReferenceFrames::snapshot() const {
  return FrameSnapshot{_state};
}

FrameVersion ReferenceFrames::apply(WorldSnapshot world,
                                    std::span<const FrameMutation> changes,
                                    FrameVersion expected) {
  if (expected != snapshot().version() || world.id() != _state->world.id() ||
      world.epoch() != _state->world.epoch() ||
      world.revision() < _state->world.revision() ||
      world.tick() < _state->world.tick())
    throw std::invalid_argument("Stale reference frame publication");
  if (changes.size() > _props.maxCommands)
    throw std::length_error("Frame command limit exceeded");
  std::size_t count = _state->records.size();
  for (const auto &change : changes) {
    if (std::holds_alternative<CreateFrame>(change)) {
      if (count == _props.maxFrames)
        throw std::length_error("Frame capacity exceeded");
      ++count;
    }
  }
  auto charge = _ledger->reserve(
      runtime::MemoryClass::CPU, runtime::ResourceKind::World, estimate(count),
      "Frame snapshot candidate", {world.id().value, world.epoch(), 0});
  auto candidate = std::make_shared<detail::FrameState>(std::move(world));
  candidate->charge = std::move(charge);
  candidate->revision = increment(_state->revision);
  candidate->records.reserve(count);
  candidate->records.assign(_state->records.begin(), _state->records.end());
  std::vector<unsigned char> discontinuities;
  discontinuities.reserve(count);
  discontinuities.resize(candidate->records.size());
  for (const auto &change : changes)
    std::visit(
        [&](const auto &command) {
          using Command = std::decay_t<decltype(command)>;
          valid(command.id, candidate->world);
          auto it = lookup(candidate->records, command.id);
          const bool exists =
              it != candidate->records.end() && it->id == command.id;
          if constexpr (std::is_same_v<Command, CreateFrame>) {
            if (exists)
              throw std::invalid_argument("Frame identity cannot be reused");
            FrameRecord record{command.id, command.props};
            const auto index = it - candidate->records.begin();
            candidate->records.insert(it, record);
            discontinuities.insert(discontinuities.begin() + index, 1);
          } else {
            if (!exists || it->removed)
              throw std::invalid_argument("Frame is missing or removed");
            if constexpr (std::is_same_v<Command, SetFrame>) {
              if (it->definition.parent != command.props.parent &&
                  !command.discontinuity)
                throw std::invalid_argument(
                    "Reparenting requires explicit discontinuity");
              it->definition = command.props;
              discontinuities[static_cast<std::size_t>(
                  it - candidate->records.begin())] |=
                  command.discontinuity ? 1 : 0;
            } else
              it->removed = true;
          }
        },
        change);
  std::vector<unsigned char> marks(count);
  std::vector<std::size_t> depths(count);
  const auto sample = [&](auto &&self, std::size_t index,
                          std::size_t recursion) -> void {
    auto &record = candidate->records[index];
    if (record.removed || marks[index] == 2)
      return;
    if (marks[index] == 1)
      throw std::invalid_argument("Reference frame cycle");
    if (recursion > _props.maxDepth)
      throw std::length_error("Reference frame depth exceeded");
    marks[index] = 1;
    const auto &limits = candidate->world.space(record.id.space)->limits;
    auto &definition = record.definition;
    definition.local.orientation =
        math::normalizedRotation(definition.local.orientation);
    if (!isFinite(definition.local.offset) || !isFinite(definition.linear) ||
        !isFinite(definition.angular))
      throw std::invalid_argument("Reference frame data must be finite");
    const bool localDiscontinuity = discontinuities[index] != 0;
    auto discontinuity = record.sample.discontinuity;
    record.sample = {record.id, candidate->world.version(),
                     candidate->world.tick(), candidate->revision};
    depths[index] = 1;
    bool changed = localDiscontinuity;
    if (definition.parent) {
      valid(*definition.parent, candidate->world);
      if (definition.parent->space != record.id.space)
        throw std::invalid_argument("Frame parent in another space");
      const auto parent = lookup(candidate->records, *definition.parent);
      if (parent == candidate->records.end() ||
          parent->id != *definition.parent || parent->removed)
        throw std::invalid_argument("Frame parent is absent or removed");
      const auto parentIndex =
          static_cast<std::size_t>(parent - candidate->records.begin());
      self(self, parentIndex, recursion + 1);
      depths[index] = depths[parentIndex] + 1;
      if (depths[index] > _props.maxDepth)
        throw std::length_error("Reference frame depth exceeded");
      changed |= discontinuities[parentIndex] != 0;
      record.sample.pose =
          compose(parent->sample.pose, definition.local, limits);
      record.sample.velocity =
          attachedVelocity(parent->sample.pose, parent->sample.velocity,
                           definition.local.offset, definition.linear);
      record.sample.velocity.angular =
          parent->sample.velocity.angular +
          rotate(parent->sample.pose.orientation, definition.angular);
    } else {
      record.sample.pose = {{record.id.space, definition.local.offset},
                            definition.local.orientation};
      record.sample.velocity = {record.id.space, definition.linear,
                                definition.angular};
      validate(record.sample.pose, limits);
      validate(record.sample.velocity);
    }
    record.sample.discontinuity =
        changed ? increment(discontinuity) : discontinuity;
    discontinuities[index] = changed;
    marks[index] = 2;
  };
  for (std::size_t i = 0; i < candidate->records.size(); ++i)
    sample(sample, i, 1);
  for (const auto &entity : candidate->world.entities()) {
    if (entity.destroyed || !entity.props.attachment)
      continue;
    const auto id = entity.props.attachment->frame;
    const auto frame = lookup(candidate->records, id);
    if (frame == candidate->records.end() || frame->id != id || frame->removed)
      throw std::invalid_argument("Live entity requires an attached frame");
  }
  candidate->charge->setState(runtime::AllocationState::Owned);
  _state = std::move(candidate);
  return snapshot().version();
}

} // namespace playground::world
