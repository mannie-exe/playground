#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

#include <world/Lifecycle.hpp>

namespace playground::world {
namespace {
void current(const WorldSnapshot &world, WorldId id, std::uint64_t epoch) {
  if (world.id() != id || world.epoch() != epoch)
    throw std::invalid_argument(
        "Lifecycle snapshot belongs to another world epoch");
}

void time(double value) {
  if (!std::isfinite(value) || value < 0)
    throw std::invalid_argument("Invalid simulation time");
}

runtime::ResourceLedger::Token
reserve(const std::shared_ptr<runtime::ResourceLedger> &ledger,
        const WorldSnapshot &world, std::size_t bytes,
        std::string_view context) {
  if (!ledger)
    throw std::invalid_argument("Lifecycle accounting required");
  return ledger->reserve(runtime::MemoryClass::CPU,
                         runtime::ResourceKind::World, bytes, context,
                         {world.id().value, world.epoch(), 2});
}
} // namespace

void ActivationPolicy::validate() const {
  if (!std::isfinite(coarseSeconds) || coarseSeconds <= 0)
    throw std::invalid_argument("Invalid coarse simulation interval");
}

ActivationScheduler::ActivationScheduler(
    const WorldSnapshot &world, std::shared_ptr<runtime::ResourceLedger> ledger,
    ActivationPolicy policy, std::size_t maximum)
    : _ledger{std::move(ledger)}, _world{world.id()}, _epoch{world.epoch()},
      _policy{policy}, _maximum{maximum} {
  policy.validate();
  if (!maximum || maximum > 65536)
    throw std::invalid_argument("Invalid activation capacity");
  _charge = reserve(_ledger, world, 2 * maximum * (sizeof(Entry) + 96),
                    "Activation scheduling and staging");
  _charge->setState(runtime::AllocationState::Owned);
}

void ActivationScheduler::wake(EntityHandle entity, double seconds) {
  time(seconds);
  if (entity.id.world != _world || entity.epoch != _epoch)
    throw std::invalid_argument("Stale activation wake");
  auto it = _entries.find(entity.id);
  if (it == _entries.end())
    throw std::invalid_argument("Wake requires an observed live entity");
  it->second.wakeAt = seconds;
}

ActivationBatch ActivationScheduler::advance(const WorldSnapshot &world,
                                             double seconds) {
  current(world, _world, _epoch);
  time(seconds);
  if (seconds < _now || world.entities().size() > _maximum ||
      world.revision() < _revision || world.tick() < _tick)
    throw std::invalid_argument("Activation time/capacity exceeded");
  ActivationBatch result;
  result.charge =
      reserve(_ledger, world, world.entities().size() * sizeof(ActivationStep),
              "Activation step output");
  result.steps.reserve(world.entities().size());
  auto candidate = _entries;
  std::erase_if(candidate, [&](const auto &pair) {
    const auto *entity = world.find(pair.first);
    return !entity || entity->destroyed;
  });
  for (const auto &entity : world.entities()) {
    if (entity.destroyed)
      continue;
    const auto mode = entity.props.activation;
    auto [it, inserted] = candidate.try_emplace(entity.id);
    auto &entry = it->second;
    if (inserted || entry.mode != mode ||
        entry.discontinuity != entity.discontinuity) {
      entry.mode = mode;
      entry.discontinuity = entity.discontinuity;
      entry.last = seconds;
    }
    const auto elapsed = seconds - entry.last;
    const bool wake = entry.wakeAt && *entry.wakeAt <= seconds;
    if (wake || (mode == Activation::Full && elapsed > 0) ||
        (mode == Activation::Coarse && elapsed >= _policy.coarseSeconds)) {
      result.steps.push_back({{entity.id, _epoch},
                              mode,
                              mode == Activation::Dormant ? 0 : elapsed,
                              wake});
      entry.last = seconds;
      if (wake)
        entry.wakeAt.reset();
    } else if (mode == Activation::Dormant)
      entry.last = seconds;
  }
  result.charge->setState(runtime::AllocationState::Owned);
  _entries.swap(candidate);
  _now = seconds;
  _revision = world.revision();
  _tick = world.tick();
  return result;
}

ZoneTracker::ZoneTracker(const WorldSnapshot &world,
                         std::span<const ZoneDefinition> zones,
                         std::shared_ptr<runtime::ResourceLedger> ledger,
                         std::size_t maximum)
    : _ledger{std::move(ledger)}, _version{world.version()}, _world{world.id()},
      _maximum{maximum} {
  if (!maximum || maximum > 65536 || zones.size() > 256)
    throw std::invalid_argument("Invalid zone tracking capacity");
  _charge = reserve(
      _ledger, world,
      2 * maximum * (sizeof(Membership) + 96 + zones.size() * sizeof(ZoneId)) +
          zones.size() * (sizeof(ZoneDefinition) + 64),
      "Zone membership and staging");
  for (const auto &zone : zones) {
    const auto *space = world.space(zone.bounds.space);
    if (!space || zone.id.world != _world || !zone.id.value)
      throw std::invalid_argument("Invalid world zone");
    zone.bounds.validate(space->limits);
    if (!_zones.emplace(zone.id, zone).second)
      throw std::invalid_argument("Duplicate zone identity");
  }
  _charge->setState(runtime::AllocationState::Owned);
}

ZoneEvents ZoneTracker::update(const WorldSnapshot &world,
                               std::span<const ZoneObservation> observations) {
  current(world, _world, _version.epoch);
  if (world.revision() < _version.revision || observations.size() > _maximum)
    throw std::invalid_argument("Stale or excessive zone observations");
  ZoneEvents result;
  result.version = world.version();
  result.charge =
      reserve(_ledger, world, _maximum * _zones.size() * sizeof(ZoneEvent),
              "Zone transition output");
  result.values.reserve(_maximum * _zones.size());
  auto candidate = _members;
  std::set<EntityId> seen;
  for (const auto &observation : observations) {
    const auto *entity = world.find(observation.entity);
    if (!entity || entity->destroyed || !seen.insert(entity->id).second ||
        observation.inside.size() > _zones.size())
      throw std::invalid_argument("Invalid zone observation");
    auto zones = observation.inside;
    std::sort(zones.begin(), zones.end());
    if (std::adjacent_find(zones.begin(), zones.end()) != zones.end())
      throw std::invalid_argument("Repeated zone membership");
    for (auto zone : zones) {
      const auto found = _zones.find(zone);
      if (found == _zones.end() ||
          found->second.bounds.space != entity->props.pose.position.space)
        throw std::invalid_argument("Foreign zone membership");
    }
    auto [it, inserted] = candidate.try_emplace(entity->id);
    if (candidate.size() > _maximum)
      throw std::length_error("Zone membership capacity exceeded");
    auto &member = it->second;
    const auto reason = inserted ? ZoneEventReason::Initial
                        : member.discontinuity != entity->discontinuity
                            ? ZoneEventReason::Teleport
                            : ZoneEventReason::Movement;
    for (auto zone : zones)
      if (!std::binary_search(member.zones.begin(), member.zones.end(), zone))
        result.values.push_back({zone, entity->id, true, reason});
    if (observation.complete) {
      for (auto zone : member.zones)
        if (!std::binary_search(zones.begin(), zones.end(), zone))
          result.values.push_back({zone, entity->id, false, reason});
    } else {
      zones.insert(zones.end(), member.zones.begin(), member.zones.end());
      std::sort(zones.begin(), zones.end());
      zones.erase(std::unique(zones.begin(), zones.end()), zones.end());
    }
    member.zones = std::move(zones);
    if (observation.complete)
      member.discontinuity = entity->discontinuity;
  }
  for (auto it = candidate.begin(); it != candidate.end();) {
    const auto *entity = world.find(it->first);
    if (entity && entity->destroyed) {
      for (auto zone : it->second.zones)
        result.values.push_back(
            {zone, entity->id, false, ZoneEventReason::Destroyed});
      it = candidate.erase(it);
    } else
      ++it;
  }
  std::sort(result.values.begin(), result.values.end(), [](auto a, auto b) {
    return std::tie(a.zone, a.entity, a.entered) <
           std::tie(b.zone, b.entity, b.entered);
  });
  result.charge->setState(runtime::AllocationState::Owned);
  _members.swap(candidate);
  _version = world.version();
  return result;
}

ZoneEvents ZoneTracker::updateBounds(const WorldSnapshot &world) {
  if (world.entities().size() > _maximum)
    throw std::length_error("Zone observation capacity exceeded");
  auto charge =
      reserve(_ledger, world,
              world.entities().size() *
                  (sizeof(ZoneObservation) + _zones.size() * sizeof(ZoneId)),
              "Zone bounds observations");
  std::vector<ZoneObservation> observations;
  for (const auto &entity : world.entities()) {
    if (entity.destroyed)
      continue;
    auto &observation = observations.emplace_back(entity.id);
    for (const auto &[id, zone] : _zones)
      if (zone.bounds.space == entity.props.pose.position.space &&
          zone.bounds.contains(entity.props.pose.position))
        observation.inside.push_back(id);
  }
  return update(world, observations);
}

struct WorldTransfer::Impl {
  struct Ticket {
    runtime::ResourceLedger::Token charge;
    TransferResult result;
    TransferRequest request;
    WorldVersion source;
    std::vector<std::uint64_t> revisions, requests;
    std::vector<WorldMutation> mutations;
    std::vector<CellLease> leases;
  };

  World &world;
  WorldStreamer *streamer;
  const ReferenceFrames *frames;
  std::shared_ptr<runtime::ResourceLedger> ledger;
  runtime::ResourceLedger::Token charge;
  std::size_t maxTickets, maxEntities;
  std::uint64_t sequence{};
  bool closed{};
  std::map<std::uint64_t, Ticket> tickets;

  void release(Ticket &ticket) noexcept {
    if (streamer)
      for (auto request : ticket.requests)
        try {
          streamer->forget(request);
        } catch (...) {
        }
    ticket.requests.clear();
    ticket.leases.clear();
  }

  void active() const {
    if (closed)
      throw std::logic_error("World transfer scope is closed");
  }

  void validateFrames(const Ticket &ticket, const WorldSnapshot &world) const {
    for (const auto &entity : ticket.request.entities)
      if (entity.destinationFrame) {
        if (!frames)
          throw std::invalid_argument("Transfer requires a frame registry");
        const auto &expected = *entity.destinationFrame;
        const auto current = frames->snapshot().resolve(expected.id);
        if (current.worldVersion.epoch != world.epoch() ||
            current.tick != world.tick() ||
            current.revision != expected.revision ||
            current.discontinuity != expected.discontinuity ||
            current.pose != expected.pose ||
            current.velocity != expected.velocity)
          throw std::invalid_argument("Transfer frame sample changed");
      }
  }
};

WorldTransfer::WorldTransfer(World &world, WorldStreamer *streamer,
                             std::shared_ptr<runtime::ResourceLedger> ledger,
                             std::size_t maximum, std::size_t entities,
                             const ReferenceFrames *frames)
    : _impl{std::make_unique<Impl>(world, streamer, frames, std::move(ledger),
                                   runtime::ResourceLedger::Token{}, maximum,
                                   entities)} {
  if (!maximum || maximum > 4096 || !entities || entities > 4096)
    throw std::invalid_argument("Invalid transfer capacity");
  _impl->charge =
      reserve(_impl->ledger, world.snapshot(),
              maximum * (sizeof(Impl::Ticket) + 64), "World transfer tickets");
  _impl->charge->setState(runtime::AllocationState::Owned);
}

WorldTransfer::~WorldTransfer() { close(); }

std::uint64_t WorldTransfer::prepare(TransferRequest request) {
  auto &s = *_impl;
  s.active();
  if (!request.authorized || request.entities.empty() ||
      request.entities.size() > s.maxEntities ||
      request.readiness.size() > s.maxEntities ||
      s.tickets.size() >= s.maxTickets)
    throw std::invalid_argument("Unauthorized or excessive transfer request");
  const auto world = s.world.snapshot();
  std::size_t bytes =
      request.entities.size() *
          (sizeof(TransferEntity) + sizeof(WorldMutation) + 32) +
      request.readiness.size() * (sizeof(CellRequest) + sizeof(CellLease) + 16);
  for (const auto &entity : request.entities) {
    const auto size = world.resolve(entity.entity).props.data.size();
    if (size > std::numeric_limits<std::size_t>::max() - bytes)
      throw std::length_error("Transfer storage overflow");
    bytes += size;
  }
  Impl::Ticket ticket;
  ticket.charge = reserve(s.ledger, world, bytes, "Prepared transfer state");
  ticket.source = world.version();
  ticket.request = std::move(request);
  s.validateFrames(ticket, world);
  std::set<EntityId> unique;
  for (const auto &entity : ticket.request.entities) {
    if (!unique.insert(entity.entity.id).second)
      throw std::invalid_argument("Repeated transfer entity");
    const auto *space = world.space(entity.destination.position.space);
    if (!space)
      throw std::invalid_argument("Unknown transfer destination");
    validate(entity.destination, space->limits);
    const auto &record = world.resolve(entity.entity);
    ticket.revisions.push_back(record.revision);
    auto props = record.props;
    props.pose = entity.destination;
    props.velocity = {space->id};
    if (entity.velocity == TransferVelocity::MapRigid) {
      const auto rotation = math::normalizedRotation(entity.velocityRotation);
      props.velocity.linear = rotate(rotation, record.props.velocity.linear);
      props.velocity.angular = rotate(rotation, record.props.velocity.angular);
    } else if (entity.velocity != TransferVelocity::Reset)
      throw std::invalid_argument("Invalid transfer velocity policy");
    props.attachment.reset();
    if (entity.destinationFrame)
      props.attachment =
          attach(*entity.destinationFrame, props.pose, props.velocity,
                 FrameVelocityPolicy::PreserveWorld, space->limits);
    ticket.mutations.emplace_back(
        SetEntity{entity.entity.id, std::move(props), true});
  }
  if (ticket.request.requirePhysicalPlacement) {
    ticket.result = {TransferStatus::Unsupported,
                     {},
                     "Physical placement adapter unavailable"};
  } else {
    try {
      if (!ticket.request.readiness.empty() && !s.streamer)
        throw std::invalid_argument("Transfer readiness requires streaming");
      for (auto required : ticket.request.readiness) {
        if (required.cell.space.world != world.id() ||
            required.epoch != world.epoch())
          throw std::invalid_argument("Foreign transfer readiness");
        if (ticket.request.deadline &&
            (!required.deadline ||
             *ticket.request.deadline < *required.deadline))
          required.deadline = ticket.request.deadline;
        ticket.requests.push_back(s.streamer->request(required));
      }
    } catch (...) {
      s.release(ticket);
      throw;
    }
  }
  if (s.sequence == std::numeric_limits<std::uint64_t>::max()) {
    s.release(ticket);
    throw std::overflow_error("Transfer identity exhausted");
  }
  ticket.charge->setState(runtime::AllocationState::Owned);
  const auto id = ++s.sequence;
  try {
    s.tickets.emplace(id, std::move(ticket));
  } catch (...) {
    s.release(ticket);
    throw;
  }
  return id;
}

TransferResult WorldTransfer::state(std::uint64_t id) const {
  return _impl->tickets.at(id).result;
}

void WorldTransfer::advance(runtime::ActivityClock::time_point now) {
  auto &s = *_impl;
  s.active();
  for (auto &[id, ticket] : s.tickets) {
    if (ticket.result.status != TransferStatus::Preparing &&
        ticket.result.status != TransferStatus::Ready)
      continue;
    try {
      const auto world = s.world.snapshot();
      current(world, ticket.request.entities.front().entity.id.world,
              ticket.source.epoch);
      if (ticket.request.deadline && now >= *ticket.request.deadline)
        throw std::invalid_argument("Transfer deadline exceeded");
      for (std::size_t i = 0; i < ticket.request.entities.size(); ++i)
        if (world.resolve(ticket.request.entities[i].entity).revision !=
            ticket.revisions[i])
          throw std::invalid_argument("Transfer source changed");
      s.validateFrames(ticket, world);
      bool ready = true;
      ticket.leases.clear();
      for (std::size_t i = 0; i < ticket.requests.size(); ++i) {
        const auto status = s.streamer->requestState(ticket.requests[i]);
        if (status.status == CellStatus::Failed ||
            status.status == CellStatus::Cancelled)
          throw std::runtime_error("Transfer readiness failed: " +
                                   status.diagnostic);
        const auto &request = ticket.request.readiness[i];
        auto lease =
            s.streamer->lease(request.cell, request.required, request.revision);
        ready = ready && bool(lease);
        ticket.leases.push_back(std::move(lease));
      }
      ticket.result.status =
          ready ? TransferStatus::Ready : TransferStatus::Preparing;
    } catch (const std::exception &error) {
      ticket.result = {TransferStatus::Failed, {}, error.what()};
      s.release(ticket);
    }
  }
}

WorldVersion WorldTransfer::commit(std::uint64_t id, std::uint64_t tick,
                                   runtime::ActivityClock::time_point now) {
  auto &s = *_impl;
  advance(now);
  auto &ticket = s.tickets.at(id);
  if (ticket.result.status != TransferStatus::Ready)
    throw std::logic_error("Transfer is not ready to commit");
  try {
    s.world.apply(ticket.mutations, s.world.snapshot().version(), tick);
    ticket.result = {
        TransferStatus::Committed, s.world.snapshot().version(), {}};
    s.release(ticket);
    return *ticket.result.committed;
  } catch (const std::exception &error) {
    ticket.result = {TransferStatus::Failed, {}, error.what()};
    s.release(ticket);
    throw;
  }
}

void WorldTransfer::cancel(std::uint64_t id) {
  auto &s = *_impl;
  s.active();
  auto &ticket = s.tickets.at(id);
  if (ticket.result.status == TransferStatus::Preparing ||
      ticket.result.status == TransferStatus::Ready) {
    ticket.result = {TransferStatus::Cancelled, {}, "Transfer cancelled"};
    s.release(ticket);
  }
}

void WorldTransfer::forget(std::uint64_t id) {
  auto &s = *_impl;
  s.active();
  auto &ticket = s.tickets.at(id);
  s.release(ticket);
  s.tickets.erase(id);
}

void WorldTransfer::close() noexcept {
  auto &s = *_impl;
  if (s.closed)
    return;
  for (auto &[id, ticket] : s.tickets) {
    if (ticket.result.status == TransferStatus::Preparing ||
        ticket.result.status == TransferStatus::Ready)
      ticket.result = {TransferStatus::Cancelled, {}, {}};
    s.release(ticket);
  }
  s.closed = true;
}
} // namespace playground::world
