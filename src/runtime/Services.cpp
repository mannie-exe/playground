#include <algorithm>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

#include <runtime/Services.hpp>

namespace playground::runtime {
namespace detail {
struct ServiceEntry {
  std::weak_ptr<ServicePumpState> pump;
  std::uint64_t id{}, scope{};
  ServiceRegistration callbacks;
  ServiceStatus status{ServiceStatus::Active};
  std::exception_ptr failure;
  bool reported{}, cleaned{};
};

struct ServicePumpState {
  ActivationLifetime lifetime;
  ServicePumpProps props;
  std::thread::id owner{std::this_thread::get_id()};
  std::vector<std::shared_ptr<ServiceEntry>> entries;
  std::function<void()> wake;
  std::uint64_t nextId{1};
  std::size_t cursor{};
  bool busy{}, closed{};
  std::optional<ActivityClock::time_point> previous;
  ServicePumpStats stats;
};
} // namespace detail

namespace {
void owner(const detail::ServicePumpState &state) {
  if (state.owner != std::this_thread::get_id())
    throw std::logic_error("Service pump accessed outside its owner thread");
}

void idle(const detail::ServicePumpState &state) {
  owner(state);
  if (state.busy)
    throw std::logic_error(
        "Service pump traversal cannot be reentered or mutated");
}

void fail(detail::ServicePumpState &state,
          detail::ServiceEntry &entry) noexcept {
  if (!entry.failure) {
    entry.failure = std::current_exception();
    ++state.stats.failures;
  }
  entry.status = ServiceStatus::Failed;
}

void cleanup(detail::ServicePumpState &state) noexcept {
  if (state.busy)
    return;
  state.busy = true;
  bool changed;
  do {
    changed = false;
    for (auto &entry : state.entries) {
      if (entry->status == ServiceStatus::Active || entry->cleaned)
        continue;
      entry->cleaned = true;
      changed = true;
      auto cancel = std::move(entry->callbacks.cancel);
      entry->callbacks.demand = {};
      entry->callbacks.advance = {};
      if (cancel) {
        try {
          cancel();
        } catch (...) {
          fail(state, *entry);
        }
      }
    }
  } while (changed);
  state.busy = false;
  std::erase_if(state.entries, [](const auto &entry) {
    return entry->cleaned && (!entry->failure || entry->reported);
  });
  if (state.entries.empty())
    state.cursor = 0;
  else
    state.cursor %= state.entries.size();
}

class Traversal {
  detail::ServicePumpState &_state;

public:
  explicit Traversal(detail::ServicePumpState &state) : _state{state} {
    idle(state);
    _state.busy = true;
  }

  ~Traversal() {
    _state.busy = false;
    cleanup(_state);
  }
};

std::uint64_t identity(detail::ServicePumpState &state) {
  if (state.nextId == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Service identities exhausted");
  return state.nextId++;
}

ServiceWorkBudget grant(ServiceWorkBudget available, ServiceWorkBudget limit) {
  return {std::min(available.operations, limit.operations),
          std::min(available.bytes, limit.bytes),
          std::min(available.messages, limit.messages)};
}
} // namespace

void ServiceWorkBudget::validate() const {
  if (!operations || !bytes || !messages)
    throw std::invalid_argument("Service work limits must be positive");
}

void ServicePumpProps::validate() const {
  if (!maxServices || !maxVisits)
    throw std::invalid_argument("Service pump limits must be positive");
  budget.validate();
}

ServiceHandle::ServiceHandle(std::shared_ptr<detail::ServiceEntry> entry)
    : _entry{std::move(entry)} {}

std::uint64_t ServiceHandle::id() const noexcept { return _entry->id; }

ServiceStatus ServiceHandle::status() const noexcept { return _entry->status; }

std::exception_ptr ServiceHandle::failure() const noexcept {
  return _entry->failure;
}

void ServiceHandle::close() noexcept {
  if (const auto state = _entry->pump.lock()) {
    if (state->owner != std::this_thread::get_id())
      std::terminate();
    if (_entry->status == ServiceStatus::Active)
      _entry->status = ServiceStatus::Closed;
    cleanup(*state);
  }
}

ServiceScope::ServiceScope(std::shared_ptr<detail::ServicePumpState> state,
                           std::uint64_t id)
    : _state{std::move(state)}, _id{id},
      _lifetime{std::make_unique<ActivationLifetime>()} {
  _lifetime->activate();
}

ServiceScope::~ServiceScope() { close(); }

ServiceScope::ServiceScope(ServiceScope &&other) noexcept
    : _state{std::move(other._state)}, _id{std::exchange(other._id, 0)},
      _lifetime{std::move(other._lifetime)} {}

ServiceScope &ServiceScope::operator=(ServiceScope &&other) noexcept {
  if (this != &other) {
    close();
    _state = std::move(other._state);
    _id = std::exchange(other._id, 0);
    _lifetime = std::move(other._lifetime);
  }
  return *this;
}

ServiceHandle ServiceScope::add(ServiceRegistration callbacks) {
  const auto state = _state.lock();
  if (!state || !_id || state->closed)
    throw std::logic_error("Service scope is closed");
  idle(*state);
  callbacks.budget.validate();
  if (callbacks.name.empty() || callbacks.name.size() > 256 ||
      !callbacks.demand || !callbacks.advance)
    throw std::invalid_argument(
        "Service requires a bounded name, demand and handler");
  if (state->entries.size() >= state->props.maxServices)
    throw std::length_error("Service registration capacity exhausted");
  auto entry = std::make_shared<detail::ServiceEntry>();
  entry->pump = state;
  entry->id = identity(*state);
  entry->scope = _id;
  entry->callbacks = std::move(callbacks);
  state->entries.push_back(entry);
  return ServiceHandle{std::move(entry)};
}

void ServiceScope::close() noexcept {
  if (_lifetime)
    _lifetime->deactivate();
  const auto id = std::exchange(_id, 0);
  if (const auto state = _state.lock(); state && id) {
    if (state->owner != std::this_thread::get_id())
      std::terminate();
    for (auto &entry : state->entries)
      if (entry->scope == id && entry->status == ServiceStatus::Active)
        entry->status = ServiceStatus::Closed;
    cleanup(*state);
  }
}

bool ServiceScope::isOpen() const noexcept {
  const auto state = _state.lock();
  return state && _id && !state->closed;
}

std::function<void()> ServiceScope::wakeCallback() const {
  const auto state = _state.lock();
  if (!state || !_id || state->closed)
    return {};
  owner(*state);
  return [token = _lifetime->token(), pumpToken = state->lifetime.token(),
          wake = state->wake] {
    if (token.isActive() && pumpToken.isActive() && wake)
      wake();
  };
}

ServicePump::ServicePump(ServicePumpProps props)
    : _state{std::make_shared<detail::ServicePumpState>()} {
  props.validate();
  _state->props = props;
  _state->lifetime.activate();
}

ServicePump::~ServicePump() { close(); }

ServiceScope ServicePump::scope() {
  idle(*_state);
  if (_state->closed)
    throw std::logic_error("Service pump is closed");
  return {_state, identity(*_state)};
}

void ServicePump::setWakeCallback(std::function<void()> wake) {
  idle(*_state);
  if (_state->closed)
    throw std::logic_error("Service pump is closed");
  _state->wake = std::move(wake);
}

ServiceDemand ServicePump::demand() {
  Traversal guard{*_state};
  ServiceDemand result;
  for (auto &entry : _state->entries) {
    if (entry->failure && !entry->reported)
      result.pending = true;
    if (entry->status != ServiceStatus::Active)
      continue;
    try {
      const auto d = entry->callbacks.demand();
      result.pending |= d.pending;
      if (d.wakeAt && (!result.wakeAt || *d.wakeAt < *result.wakeAt))
        result.wakeAt = d.wakeAt;
    } catch (...) {
      fail(*_state, *entry);
      result.pending = true;
    }
  }
  return result;
}

ServiceWork ServicePump::advance(ActivityClock::time_point now) {
  Traversal guard{*_state};
  if (_state->previous && now < *_state->previous)
    throw std::invalid_argument("Service clock cannot go backwards");
  _state->previous = now;
  auto available = _state->props.budget;
  const auto count = _state->entries.size();
  const auto start = _state->cursor;
  std::size_t visited{};
  for (; visited < count && visited < _state->props.maxVisits &&
         available.operations;
       ++visited) {
    const auto index = (start + visited) % count;
    auto &entry = _state->entries[index];
    _state->cursor = (index + 1) % count;
    if (entry->status != ServiceStatus::Active)
      continue;
    try {
      const auto demand = entry->callbacks.demand();
      if (entry->status != ServiceStatus::Active || !demand.due(now))
        continue;
      const auto budget = grant(available, entry->callbacks.budget);
      ++_state->stats.visits;
      const auto used = entry->callbacks.advance(now, budget);
      if (used.operations > budget.operations || used.bytes > budget.bytes ||
          used.messages > budget.messages)
        throw std::length_error("Service exceeded its granted work budget");
      available.operations -= used.operations;
      available.bytes -= used.bytes;
      available.messages -= used.messages;
    } catch (...) {
      fail(*_state, *entry);
    }
  }
  const ServiceWork used{_state->props.budget.operations - available.operations,
                         _state->props.budget.bytes - available.bytes,
                         _state->props.budget.messages - available.messages};
  _state->stats.consumed.operations += used.operations;
  _state->stats.consumed.bytes += used.bytes;
  _state->stats.consumed.messages += used.messages;
  return used;
}

std::vector<ServiceFailure> ServicePump::takeFailures() {
  idle(*_state);
  std::vector<ServiceFailure> result;
  result.reserve(_state->entries.size());
  for (auto &entry : _state->entries)
    if (entry->failure && !entry->reported)
      result.push_back({entry->id, entry->callbacks.name, entry->failure});
  for (auto &entry : _state->entries)
    if (entry->failure)
      entry->reported = true;
  cleanup(*_state);
  return result;
}

ServicePumpStats ServicePump::stats() const {
  owner(*_state);
  return _state->stats;
}

void ServicePump::close() noexcept {
  if (_state->owner != std::this_thread::get_id())
    std::terminate();
  _state->lifetime.deactivate();
  _state->closed = true;
  for (auto &entry : _state->entries)
    if (entry->status == ServiceStatus::Active)
      entry->status = ServiceStatus::Closed;
  cleanup(*_state);
}

} // namespace playground::runtime
