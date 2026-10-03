#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <world/World.hpp>

namespace playground::world {
namespace detail {
struct WorldState {
  runtime::ResourceLedger::Token charge;
  WorldId id;
  std::uint64_t epoch{}, revision{}, tick{};
  std::vector<SpaceDefinition> spaces;
  std::vector<EntityRecord> entities;
};
} // namespace detail

namespace {
std::uint64_t nextEpoch() {
  static std::atomic<std::uint64_t> next{1};
  auto value = next.load(std::memory_order_relaxed);
  for (;;) {
    if (value == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("World epoch exhausted");
    if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
      return value;
  }
}

std::uint64_t increment(std::uint64_t value) {
  if (value == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("World revision exhausted");
  return value + 1;
}

std::size_t add(std::size_t a, std::size_t b) {
  if (a > std::numeric_limits<std::size_t>::max() - b)
    throw std::length_error("World storage estimate overflow");
  return a + b;
}

std::size_t arrayBytes(std::size_t count, std::size_t size) {
  if (count > std::numeric_limits<std::size_t>::max() / size)
    throw std::length_error("World array estimate overflow");
  return count * size;
}

template <class Records, class Id> auto lookup(Records &records, Id id) {
  return std::lower_bound(records.begin(), records.end(), id,
                          [](const auto &r, auto key) { return r.id < key; });
}

void own(WorldId owner, SpaceId id) {
  validate(id);
  if (id.world != owner)
    throw std::invalid_argument("Foreign world space");
}

void own(WorldId owner, EntityId id) {
  validate(id);
  if (id.world != owner)
    throw std::invalid_argument("Foreign world entity");
}

std::size_t stateBytes(const detail::WorldState &state) {
  auto bytes = add(sizeof(detail::WorldState),
                   arrayBytes(state.spaces.size(), sizeof(SpaceDefinition)));
  bytes = add(bytes, arrayBytes(state.entities.size(), sizeof(EntityRecord)));
  for (const auto &e : state.entities)
    bytes = add(bytes, e.props.data.size());
  return bytes;
}

void validProps(const detail::WorldState &state, const EntityProps &p,
                const WorldProps &limits) {
  own(state.id, p.pose.position.space);
  const auto it = lookup(state.spaces, p.pose.position.space);
  if (it == state.spaces.end() || it->id != p.pose.position.space)
    throw std::invalid_argument("Entity space has not been defined");
  validate(p.pose, it->limits);
  validate(p.velocity);
  if (p.velocity.space != p.pose.position.space)
    throw std::invalid_argument("Entity pose and velocity spaces differ");
  switch (p.activation) {
  case Activation::Dormant:
  case Activation::Coarse:
  case Activation::Full:
    break;
  default:
    throw std::invalid_argument("Invalid entity activation");
  }
  if (p.data.size() > limits.maxEntityBytes)
    throw std::length_error("Entity domain data exceeds limit");
}

auto candidate(const detail::WorldState &source,
               std::shared_ptr<runtime::ResourceLedger> ledger,
               std::size_t bytes, std::size_t spaces, std::size_t entities,
               const WorldProps &limits,
               std::optional<std::uint64_t> epoch = {}) {
  if (spaces > limits.maxSpaces || entities > limits.maxEntities ||
      bytes > limits.maxStateBytes)
    throw std::length_error("World candidate exceeds state limits");
  auto charge =
      ledger->reserve(runtime::MemoryClass::CPU, runtime::ResourceKind::World,
                      bytes, "World snapshot candidate",
                      {source.id.value, epoch.value_or(source.epoch), 0});
  auto result = std::make_shared<detail::WorldState>();
  result->spaces.reserve(spaces);
  result->entities.reserve(entities);
  result->id = source.id;
  result->epoch = epoch.value_or(source.epoch);
  result->revision = source.revision;
  result->tick = source.tick;
  result->spaces = source.spaces;
  result->entities = source.entities;
  result->charge = std::move(charge);
  return result;
}
} // namespace

void WorldProps::validate() const {
  if (!maxSpaces || !maxEntities || !maxCommands || !maxEntityBytes ||
      maxStateBytes < sizeof(detail::WorldState))
    throw std::invalid_argument("World limits must admit a bounded state");
}

WorldSnapshot::WorldSnapshot(std::shared_ptr<const detail::WorldState> state)
    : _state{std::move(state)} {}

WorldId WorldSnapshot::id() const noexcept { return _state->id; }

std::uint64_t WorldSnapshot::epoch() const noexcept { return _state->epoch; }

std::uint64_t WorldSnapshot::revision() const noexcept {
  return _state->revision;
}

std::uint64_t WorldSnapshot::tick() const noexcept { return _state->tick; }

WorldVersion WorldSnapshot::version() const noexcept {
  return {epoch(), revision()};
}

std::span<const SpaceDefinition> WorldSnapshot::spaces() const noexcept {
  return _state->spaces;
}

std::span<const EntityRecord> WorldSnapshot::entities() const noexcept {
  return _state->entities;
}

std::size_t WorldSnapshot::reservedBytes() const noexcept {
  return _state->charge->bytes();
}

const SpaceDefinition *WorldSnapshot::space(SpaceId id) const {
  own(_state->id, id);
  const auto it = lookup(_state->spaces, id);
  return it == _state->spaces.end() || it->id != id ? nullptr : &*it;
}

const EntityRecord *WorldSnapshot::find(EntityId id) const {
  own(_state->id, id);
  const auto it = lookup(_state->entities, id);
  return it == _state->entities.end() || it->id != id ? nullptr : &*it;
}

const EntityRecord &WorldSnapshot::resolve(EntityHandle handle) const {
  if (handle.epoch != epoch())
    throw std::invalid_argument("Entity handle belongs to another world epoch");
  const auto *result = find(handle.id);
  if (!result || result->destroyed)
    throw std::invalid_argument("Entity handle is not live in this snapshot");
  return *result;
}

World::World(WorldId id, std::shared_ptr<runtime::ResourceLedger> ledger,
             WorldProps props)
    : _props{props}, _ledger{std::move(ledger)} {
  props.validate();
  if (!id.value || !_ledger)
    throw std::invalid_argument(
        "World requires identity and resource accounting");
  detail::WorldState empty;
  empty.id = id;
  empty.epoch = nextEpoch();
  auto state = candidate(empty, _ledger, sizeof(empty), 0, 0, _props);
  state->charge->setState(runtime::AllocationState::Owned);
  _state = std::move(state);
}

WorldSnapshot World::snapshot() const { return WorldSnapshot{_state}; }

EntityHandle World::handle(EntityId id) const {
  const EntityHandle result{id, _state->epoch};
  snapshot().resolve(result);
  return result;
}

std::uint64_t World::apply(std::span<const WorldMutation> mutations,
                           WorldVersion expected, std::uint64_t tick) {
  if (expected != snapshot().version())
    throw std::invalid_argument("World epoch/revision conflict");
  if (tick < _state->tick)
    throw std::invalid_argument("World tick cannot go backwards");
  if (mutations.size() > _props.maxCommands)
    throw std::length_error("World command batch exceeds limit");
  if (mutations.empty() && tick == _state->tick)
    return _state->revision;

  // Payload sizes follow command order; repeated replacements must not charge
  // obsolete payloads to the final snapshot. Account the bounded sizing table
  // separately, before allocating it.
  struct PayloadSize {
    EntityId id;
    std::size_t bytes{};
  };

  const auto sizingCharge =
      mutations.empty()
          ? runtime::ResourceLedger::Token{}
          : _ledger->reserve(
                runtime::MemoryClass::CPU, runtime::ResourceKind::World,
                arrayBytes(mutations.size(), sizeof(PayloadSize)),
                "World mutation sizing", {_state->id.value, _state->epoch, 0});
  std::vector<PayloadSize> payloads;
  payloads.reserve(mutations.size());
  auto bytes = stateBytes(*_state);
  auto spaces = _state->spaces.size(), entities = _state->entities.size();
  for (const auto &mutation : mutations)
    std::visit(
        [&](const auto &m) {
          using T = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<T, CreateSpace>) {
            spaces = add(spaces, 1);
            bytes = add(bytes, sizeof(SpaceDefinition));
          } else {
            payloads.push_back({m.id});
            if constexpr (!std::is_same_v<T, DestroyEntity>) {
              if (m.props.data.size() > _props.maxEntityBytes)
                throw std::length_error("Entity domain data exceeds limit");
            }
            if constexpr (std::is_same_v<T, SpawnEntity>) {
              entities = add(entities, 1);
              bytes = add(bytes, sizeof(EntityRecord));
            }
          }
        },
        mutation);
  std::sort(payloads.begin(), payloads.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  payloads.erase(
      std::unique(payloads.begin(), payloads.end(),
                  [](const auto &a, const auto &b) { return a.id == b.id; }),
      payloads.end());
  for (auto &payload : payloads) {
    const auto found = lookup(_state->entities, payload.id);
    if (found != _state->entities.end() && found->id == payload.id)
      payload.bytes = found->props.data.size();
  }
  // Structural capacity is reserved up front. Replacement copies temporarily
  // coexist with the candidate's preceding payload; immutable readers retain
  // their own independent charges throughout publication.
  auto peakBytes = bytes;
  for (const auto &mutation : mutations)
    std::visit(
        [&](const auto &m) {
          using T = std::decay_t<decltype(m)>;
          if constexpr (!std::is_same_v<T, CreateSpace>) {
            auto &previous = lookup(payloads, m.id)->bytes;
            if constexpr (std::is_same_v<T, DestroyEntity>) {
              bytes -= previous;
              previous = 0;
            } else {
              const auto replacement = m.props.data.size();
              peakBytes = std::max(peakBytes, add(bytes, replacement));
              if constexpr (std::is_same_v<T, SetEntity>)
                bytes -= previous;
              bytes = add(bytes, replacement);
              previous = replacement;
            }
          }
        },
        mutation);
  if (spaces > _props.maxSpaces || entities > _props.maxEntities ||
      bytes > _props.maxStateBytes)
    throw std::length_error("World candidate exceeds state limits");
  const auto stagingCharge =
      peakBytes == bytes
          ? runtime::ResourceLedger::Token{}
          : _ledger->reserve(runtime::MemoryClass::CPU,
                             runtime::ResourceKind::World, peakBytes - bytes,
                             "World payload replacement staging",
                             {_state->id.value, _state->epoch, 0});
  auto next = candidate(*_state, _ledger, bytes, spaces, entities, _props);
  next->revision = increment(_state->revision);
  next->tick = tick;
  for (const auto &mutation : mutations)
    std::visit(
        [&](const auto &m) {
          using T = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<T, CreateSpace>) {
            own(next->id, m.definition.id);
            m.definition.limits.validate();
            const auto pos = lookup(next->spaces, m.definition.id);
            if (pos != next->spaces.end() && pos->id == m.definition.id)
              throw std::invalid_argument("Duplicate world space");
            next->spaces.insert(pos, m.definition);
          } else {
            own(next->id, m.id);
            auto pos = lookup(next->entities, m.id);
            const bool found = pos != next->entities.end() && pos->id == m.id;
            if constexpr (std::is_same_v<T, SpawnEntity>) {
              if (found)
                throw std::invalid_argument("Entity identity cannot be reused");
              validProps(*next, m.props, _props);
              pos = next->entities.insert(
                  pos, {m.id, m.props, next->revision, 1, false});
              pos->props.pose.orientation =
                  math::normalizedRotation(pos->props.pose.orientation);
            } else {
              if (!found || pos->destroyed)
                throw std::invalid_argument("Entity is not live");
              if constexpr (std::is_same_v<T, DestroyEntity>) {
                pos->destroyed = true;
                std::vector<std::byte>{}.swap(pos->props.data);
                pos->discontinuity = increment(pos->discontinuity);
              } else {
                validProps(*next, m.props, _props);
                if (pos->props.pose.position.space !=
                        m.props.pose.position.space &&
                    !m.discontinuity)
                  throw std::invalid_argument(
                      "Space transfer requires a discontinuity");
                auto props = m.props;
                props.pose.orientation =
                    math::normalizedRotation(props.pose.orientation);
                pos->props = std::move(props);
                if (m.discontinuity)
                  pos->discontinuity = increment(pos->discontinuity);
              }
              pos->revision = next->revision;
            }
          }
        },
        mutation);
  next->charge->setState(runtime::AllocationState::Owned);
  _state = std::move(next);
  return _state->revision;
}

void World::restore(const WorldSnapshot &source, WorldVersion expected) {
  if (source.id() != _state->id || expected != snapshot().version())
    throw std::invalid_argument("World restore identity/revision conflict");
  for (const auto &e : source.entities())
    validProps(*source._state, e.props, _props);
  auto next = candidate(*source._state, _ledger, stateBytes(*source._state),
                        source.spaces().size(), source.entities().size(),
                        _props, nextEpoch());
  next->charge->setState(runtime::AllocationState::Owned);
  _state = std::move(next);
}

} // namespace playground::world
