#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>

#include <world/Navigation.hpp>

namespace playground::world {
struct NavigationService::Impl {
  struct Slot {
    runtime::ResourceLedger::Token charge;
    NavigationSnapshot source;
    std::mutex mutex;
    NavigationResult result;
    bool complete{};

    explicit Slot(NavigationSnapshot snapshot) : source{std::move(snapshot)} {}
  };

  struct Entry {
    NavigationRequest request;
    NavigationResult result;
    std::shared_ptr<Slot> slot;
    std::optional<runtime::TaskTicket> task;
  };

  runtime::ResourceLedger::Token charge;
  NavigationSnapshot snapshot;
  runtime::Executor &executor;
  std::shared_ptr<runtime::ResourceLedger> ledger;
  std::size_t maximum;
  std::map<std::uint64_t, Entry> entries;
  std::uint64_t sequence{}, cursor{};
  bool closed{};
  runtime::ActivityClock::time_point now{};
  std::function<void()> wake;

  Impl(NavigationSnapshot value, runtime::Executor &worker,
       std::shared_ptr<runtime::ResourceLedger> account, std::size_t count)
      : snapshot{std::move(value)}, executor{worker},
        ledger{std::move(account)}, maximum{count} {
    if (!ledger || !maximum || maximum > 4096)
      throw std::invalid_argument("Invalid navigation service capacity");
    charge = admission(snapshot);
    charge->setState(runtime::AllocationState::Owned);
  }

  runtime::ResourceLedger::Token admission(const NavigationSnapshot &value) {
    return ledger->reserve(
        runtime::MemoryClass::CPU, runtime::ResourceKind::Navigation,
        maximum * (sizeof(Entry) + 512), "Navigation request slots",
        {value.world().value, value.worldVersion().epoch, 3});
  }

  void active() const {
    if (closed)
      throw std::logic_error("Navigation scope is closed");
  }

  void notify() noexcept {
    if (wake)
      try {
        wake();
      } catch (...) {
      }
  }

  void cancel(Entry &entry) noexcept {
    if (entry.task)
      entry.task->cancel();
    if (entry.result.status == NavigationStatus::Planning) {
      entry.result.status = NavigationStatus::Cancelled;
      entry.result.path.reset();
    }
  }

  void close() noexcept {
    if (closed)
      return;
    closed = true;
    for (auto &[id, entry] : entries) {
      cancel(entry);
      entry.slot.reset();
    }
    wake = {};
  }
};

NavigationService::NavigationService(
    NavigationSnapshot snapshot, runtime::Executor &executor,
    std::shared_ptr<runtime::ResourceLedger> ledger, std::size_t maximum)
    : _impl{std::make_shared<Impl>(std::move(snapshot), executor,
                                   std::move(ledger), maximum)} {}

NavigationService::~NavigationService() { close(); }

void NavigationService::setSnapshot(NavigationSnapshot snapshot) {
  auto &s = *_impl;
  s.active();
  if (snapshot.world() != s.snapshot.world() ||
      (snapshot.worldVersion().epoch == s.snapshot.worldVersion().epoch &&
       (snapshot.worldVersion().revision < s.snapshot.worldVersion().revision ||
        snapshot.tick() < s.snapshot.tick())))
    throw std::invalid_argument("Stale/foreign navigation snapshot");
  auto charge = snapshot.worldVersion().epoch == s.snapshot.worldVersion().epoch
                    ? s.charge
                    : s.admission(snapshot);
  charge->setState(runtime::AllocationState::Owned);
  if (snapshot.worldVersion().epoch != s.snapshot.worldVersion().epoch)
    for (auto &[id, entry] : s.entries)
      if (entry.result.status == NavigationStatus::Planning)
        s.cancel(entry);
  s.snapshot = std::move(snapshot);
  s.charge = std::move(charge);
  s.notify();
}

std::uint64_t NavigationService::request(NavigationRequest request) {
  auto &s = *_impl;
  s.active();
  request.profile.validate();
  request.budget.validate();
  if (request.agent.id.world != s.snapshot.world() ||
      request.agent.epoch != s.snapshot.worldVersion().epoch ||
      !request.goalRevision)
    throw std::invalid_argument("Stale navigation request");
  if (s.entries.size() >= s.maximum ||
      s.sequence == std::numeric_limits<std::uint64_t>::max())
    throw std::length_error("Navigation request capacity/identity exhausted");
  for (const auto &[id, entry] : s.entries)
    if (entry.request.agent == request.agent &&
        entry.request.goalRevision > request.goalRevision)
      throw std::invalid_argument("Navigation goal revision moved backwards");
  const auto id = ++s.sequence;
  s.entries.emplace(id, Impl::Entry{request});
  for (auto &[other, entry] : s.entries)
    if (other != id && entry.request.agent == request.agent &&
        entry.result.status == NavigationStatus::Planning)
      s.cancel(entry);
  s.notify();
  return id;
}

void NavigationService::cancel(std::uint64_t id) {
  _impl->active();
  _impl->cancel(_impl->entries.at(id));
  _impl->notify();
}

void NavigationService::forget(std::uint64_t id) {
  auto &s = *_impl;
  s.active();
  s.cancel(s.entries.at(id));
  s.entries.erase(id);
}

NavigationResult NavigationService::poll(std::uint64_t id) const {
  return _impl->entries.at(id).result;
}

runtime::ServiceWork
NavigationService::advanceImpl(Impl &s, runtime::ActivityClock::time_point now,
                               runtime::ServiceWorkBudget budget) {
  if (s.closed)
    return {};
  if (now < s.now)
    throw std::invalid_argument("Navigation clock moved backwards");
  s.now = now;
  runtime::ServiceWork work;
  for (std::size_t visited = 0;
       visited < s.entries.size() && work.operations < budget.operations;
       ++visited) {
    auto it = s.entries.upper_bound(s.cursor);
    if (it == s.entries.end())
      it = s.entries.begin();
    s.cursor = it->first;
    auto &entry = it->second;
    if (entry.task && entry.task->retired()) {
      if (work.messages >= budget.messages ||
          budget.bytes - work.bytes < sizeof(NavigationResult))
        continue;
      ++work.operations;
      ++work.messages;
      work.bytes += sizeof(NavigationResult);
      if (entry.result.status == NavigationStatus::Planning) {
        std::lock_guard lock{entry.slot->mutex};
        if (entry.slot->complete)
          entry.result = std::move(entry.slot->result);
        else {
          entry.result.status = NavigationStatus::Failed;
          entry.result.diagnostic = "Navigation worker retired without result";
        }
        if (!s.snapshot.accepts(entry.request.agent) ||
            (entry.result.path
                 ? !entry.result.path->valid(s.snapshot)
                 : !entry.slot->source.sameTopology(s.snapshot))) {
          entry.result.status = NavigationStatus::Cancelled;
          entry.result.path.reset();
        }
      }
      entry.slot.reset();
      entry.task.reset();
    }
    if (entry.task || entry.result.status != NavigationStatus::Planning)
      continue;
    if (work.operations >= budget.operations ||
        budget.bytes - work.bytes < sizeof(NavigationRequest))
      continue;
    ++work.operations;
    try {
      auto charge = s.ledger->reserve(
          runtime::MemoryClass::CPU, runtime::ResourceKind::Navigation,
          sizeof(Impl::Slot) + sizeof(NavigationRequest) + 128,
          "Navigation worker slot",
          {s.snapshot.world().value, s.snapshot.worldVersion().epoch, 3});
      auto slot = std::make_shared<Impl::Slot>(s.snapshot);
      slot->charge = std::move(charge);
      auto task = s.executor.submit(
          [slot, snapshot = s.snapshot, request = entry.request,
           ledger = s.ledger, wake = s.wake](std::stop_token stop) noexcept {
            NavigationResult result;
            try {
              result = planNavigation(snapshot, request, ledger, stop);
            } catch (...) {
              result.status = NavigationStatus::Failed;
            }
            {
              std::lock_guard lock{slot->mutex};
              slot->result = std::move(result);
              slot->complete = true;
            }
            if (wake)
              try {
                wake();
              } catch (...) {
              }
          },
          sizeof(Impl::Slot) + sizeof(NavigationRequest));
      if (!task)
        continue;
      entry.slot = std::move(slot);
      entry.task = std::move(task);
      work.bytes += sizeof(NavigationRequest);
    } catch (const runtime::ResourcePressure &error) {
      entry.result.status = NavigationStatus::BudgetExceeded;
      entry.result.diagnostic = error.what();
    } catch (const std::exception &error) {
      entry.result.status = NavigationStatus::Failed;
      entry.result.diagnostic = error.what();
    }
  }
  return work;
}

runtime::ServiceDemand NavigationService::demandImpl(const Impl &s) {
  if (s.closed)
    return {};
  runtime::ServiceDemand result;
  for (const auto &[id, entry] : s.entries) {
    if (entry.task) {
      result.pending = result.pending || entry.task->retired();
      result.wakeAt = s.now + std::chrono::milliseconds{10};
    } else if (entry.result.status == NavigationStatus::Planning)
      result.wakeAt = s.now + std::chrono::milliseconds{10};
  }
  return result;
}

runtime::ServiceWork
NavigationService::advance(runtime::ActivityClock::time_point now,
                           runtime::ServiceWorkBudget budget) {
  return advanceImpl(*_impl, now, budget);
}

runtime::ServiceDemand NavigationService::demand() const {
  return demandImpl(*_impl);
}

runtime::ServiceHandle NavigationService::attach(runtime::ServiceScope &scope) {
  _impl->active();
  _impl->wake = scope.wakeCallback();
  auto weak = std::weak_ptr<Impl>{_impl};
  return scope.add({.name = "world navigation",
                    .demand =
                        [weak] {
                          if (auto s = weak.lock())
                            return demandImpl(*s);
                          return runtime::ServiceDemand{};
                        },
                    .advance =
                        [weak](auto now, auto budget) {
                          if (auto s = weak.lock())
                            return advanceImpl(*s, now, budget);
                          return runtime::ServiceWork{};
                        },
                    .cancel =
                        [weak] {
                          if (auto s = weak.lock())
                            s->close();
                        }});
}

void NavigationService::close() noexcept { _impl->close(); }

void PathFollowerProps::validate() const {
  if (!std::isfinite(speed) || speed <= 0 ||
      !std::isfinite(waypointTolerance) || waypointTolerance < 0 ||
      !std::isfinite(stallSeconds) || stallSeconds <= 0 ||
      !std::isfinite(progressMeters) || progressMeters <= 0)
    throw std::invalid_argument("Invalid path following configuration");
}

PathFollower::PathFollower(PathFollowerProps props) : _props{props} {
  props.validate();
}

void PathFollower::follow(std::shared_ptr<const NavigationPath> path) {
  if (!path || path->points.empty())
    throw std::invalid_argument("Path follower requires a corridor");
  _path = std::move(path);
  _next = 0;
  _stalled = 0;
  _bestDistance = std::numeric_limits<double>::infinity();
  _traversing.reset();
  _traversalCompleted = false;
  _state = NavigationState::Following;
  _discontinuity.reset();
  _tick.reset();
}

void PathFollower::cancel() {
  _state = NavigationState::Cancelled;
  _path.reset();
  _traversing.reset();
}

void PathFollower::completeTraversal(NavigationLinkId link, bool success) {
  if (_state != NavigationState::Traversing || _traversing != link)
    throw std::invalid_argument("Stale navigation traversal completion");
  _traversalCompleted = success;
  _state = success ? NavigationState::Following : NavigationState::Blocked;
}

PathFollowResult PathFollower::advance(const EntitySample &actual,
                                       double seconds,
                                       const NavigationSnapshot &snapshot,
                                       const FrameSnapshot *frames) {
  validate(actual.pose);
  validate(actual.velocity);
  if (!std::isfinite(seconds) || seconds < 0 ||
      actual.velocity.space != actual.pose.position.space)
    throw std::invalid_argument("Invalid path follower sample/time");
  PathFollowResult result{_state, {actual.pose.position.space}};
  if (!_path || _state == NavigationState::Cancelled ||
      _state == NavigationState::Blocked || _state == NavigationState::Failed)
    return result;
  if (_tick && actual.tick < *_tick)
    throw std::invalid_argument("Path follower tick moved backwards");
  _tick = actual.tick;
  if (_discontinuity && *_discontinuity != actual.discontinuity &&
      !_traversalCompleted) {
    _path.reset();
    return {_state = NavigationState::WaitingForData,
            {actual.pose.position.space}};
  }
  _discontinuity = actual.discontinuity;
  if (actual.entity != _path->agent)
    return {_state = NavigationState::Failed, {actual.pose.position.space}};
  if (!_path->valid(snapshot))
    return {_state = NavigationState::WaitingForData,
            {actual.pose.position.space}};
  const auto position = [&](const NavigationWaypoint &waypoint) {
    if (!waypoint.frame)
      return waypoint.position;
    if (!frames || frames->world().epoch() != actual.entity.epoch ||
        frames->world().tick() != actual.tick)
      throw std::invalid_argument("Following requires a matching frame tick");
    const auto &sample = frames->resolve(waypoint.frame->frame);
    if (sample.discontinuity != waypoint.frameDiscontinuity)
      throw std::invalid_argument("Navigation frame changed discontinuously");
    return worldPosition(*waypoint.frame, sample);
  };
  try {
    while (_next < _path->points.size()) {
      const auto &point = _path->points[_next];
      if (point.traversal != TraversalKind::Walk && !_traversalCompleted) {
        _state = NavigationState::Traversing;
        _traversing = point.link;
        return {_state, {actual.pose.position.space}, {}, point};
      }
      const auto target = position(point);
      if (target.space != actual.pose.position.space)
        return {_state = NavigationState::Blocked,
                {actual.pose.position.space}};
      const auto delta = relativeTo(target, actual.pose.position);
      const auto distance = length(delta);
      const auto tolerance = _next + 1 == _path->points.size()
                                 ? _path->goalTolerance
                                 : _props.waypointTolerance;
      if (_traversalCompleted && distance > tolerance)
        return {_state = NavigationState::Blocked,
                {actual.pose.position.space}};
      if (distance <= tolerance) {
        ++_next;
        _stalled = 0;
        _bestDistance = std::numeric_limits<double>::infinity();
        _traversing.reset();
        _traversalCompleted = false;
        continue;
      }
      if (distance + _props.progressMeters <= _bestDistance) {
        _bestDistance = distance;
        _stalled = 0;
      } else
        _stalled += seconds;
      if (_stalled >= _props.stallSeconds)
        return {_state = NavigationState::Blocked,
                {actual.pose.position.space}};
      const auto speed =
          seconds > 0 ? std::min(_props.speed, distance / seconds) : 0;
      result.state = _state = NavigationState::Following;
      result.desired.linear = delta * (speed / distance);
      result.facingRadians = std::atan2(delta.x, delta.z);
      return result;
    }
    result.state = _state = _path->complete ? NavigationState::Arrived
                                            : NavigationState::WaitingForData;
    return result;
  } catch (const std::invalid_argument &) {
    return {_state = NavigationState::WaitingForData,
            {actual.pose.position.space}};
  }
}
} // namespace playground::world
