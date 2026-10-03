#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>

#include <world/Streaming.hpp>

namespace playground::world {
namespace detail {
struct CellGeneration {
  runtime::ResourceLedger::Token charge;
  CellId cell;
  std::uint64_t revision{}, contentGeneration{};
  std::shared_ptr<const CellProduct> product;
};
} // namespace detail

namespace {
using Clock = runtime::ActivityClock;
constexpr auto retryDelay = std::chrono::milliseconds{10};

void valid(Readiness value, bool allowEmpty = false) {
  if ((!allowEmpty && value == Readiness::None) || (unsigned(value) & ~31u))
    throw std::invalid_argument("Invalid cell readiness");
}

std::size_t add(std::size_t a, std::size_t b) {
  if (b > std::numeric_limits<std::size_t>::max() - a)
    throw std::length_error("Streaming storage estimate overflow");
  return a + b;
}

double distance(WorldPosition position, const WorldBounds &box) {
  if (position.space != box.space)
    return std::numeric_limits<double>::infinity();
  const auto p = position.meters;
  return std::hypot(
      std::hypot(p.x - std::clamp(p.x, box.minimum.x, box.maximum.x),
                 p.y - std::clamp(p.y, box.minimum.y, box.maximum.y)),
      p.z - std::clamp(p.z, box.minimum.z, box.maximum.z));
}

std::uint64_t next(std::uint64_t &value) {
  if (value == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Streaming generation exhausted");
  return ++value;
}
} // namespace

CellLease::CellLease(std::shared_ptr<const detail::CellGeneration> generation)
    : _generation{std::move(generation)} {}

const CellProduct &CellLease::product() const {
  if (!_generation)
    throw std::logic_error("Empty cell lease");
  return *_generation->product;
}

CellId CellLease::cell() const {
  if (!_generation)
    throw std::logic_error("Empty cell lease");
  return _generation->cell;
}

std::uint64_t CellLease::revision() const {
  if (!_generation)
    throw std::logic_error("Empty cell lease");
  return _generation->revision;
}

void StreamingProps::validate() const {
  if (!maxCells || maxCells > 65536 || !maxSources || maxSources > 1024 ||
      !maxRequests || maxRequests > 65536 || !maxJobs || maxJobs > 64 ||
      !maxRetiring || maxRetiring > 65536 || !productBytes || !scratchBytes ||
      !std::isfinite(minimumRetentionSeconds) || minimumRetentionSeconds < 0 ||
      !std::isfinite(agingSeconds) || agingSeconds <= 0 ||
      !std::isfinite(maximumPredictionSeconds) || maximumPredictionSeconds < 0)
    throw std::invalid_argument("Invalid streaming limits");
  add(add(productBytes, scratchBytes), sizeof(detail::CellGeneration));
}

struct WorldStreamer::Impl {
  struct Slot {
    runtime::ResourceLedger::Token charge;
    std::mutex mutex;
    std::shared_ptr<const CellProduct> result;
    std::exception_ptr error;
    bool completed{};
    double seconds{};
    CellRequest request;
    std::uint64_t contentGeneration{};
    std::vector<std::shared_ptr<const detail::CellGeneration>> dependencies;
    std::optional<runtime::TaskTicket> ticket;
  };

  struct Entry {
    CellDefinition definition;
    Readiness requested{Readiness::None};
    int priority{};
    bool pinned{};
    std::uint64_t generation{}, contentGeneration{1};
    CellStatus status{CellStatus::Absent};
    std::string diagnostic;
    Clock::time_point queued{}, retainedUntil{}, retryAt{};
    std::shared_ptr<const detail::CellGeneration> current;
    std::shared_ptr<Slot> job;
  };

  struct Sources {
    std::vector<StreamingSource> values;
    std::set<CellId> retained;
  };

  struct Request {
    CellRequest value;
    std::optional<CellStatus> terminal;
    std::string diagnostic;
  };

  runtime::ResourceLedger::Token metadata;
  WorldId world;
  std::uint64_t epoch{}, sequence{};
  StreamingProps props;
  std::shared_ptr<CellProvider> provider;
  runtime::Executor &executor;
  std::shared_ptr<runtime::ResourceLedger> ledger;
  std::function<void()> wake;
  std::map<CellId, Entry> entries;
  std::map<SpaceId, SpatialLimits> spaces;
  std::vector<CellId> order;
  std::map<std::uint64_t, Sources> sources;
  std::map<std::uint64_t, Request> requests;
  std::vector<std::weak_ptr<const detail::CellGeneration>> retired;
  StreamingStats counters;
  Clock::time_point now{};
  bool closed{}, started{};

  Impl(WorldManifest manifest, std::uint64_t incarnation,
       std::shared_ptr<CellProvider> source, runtime::Executor &worker,
       std::shared_ptr<runtime::ResourceLedger> account, StreamingProps limits,
       std::function<void()> notify)
      : world{manifest.world}, epoch{incarnation}, props{limits},
        provider{std::move(source)}, executor{worker},
        ledger{std::move(account)}, wake{std::move(notify)} {
    props.validate();
    if (!world.value || !epoch || !manifest.revision || !provider || !ledger ||
        manifest.cells.size() > props.maxCells ||
        manifest.contentLock.size() > 4096)
      throw std::invalid_argument("Invalid streaming manifest or provider");
    if (manifest.spaces.size() > props.maxCells)
      throw std::length_error("Too many streaming spaces");
    for (const auto &space : manifest.spaces) {
      validate(space.id);
      space.limits.validate();
      if (space.id.world != world ||
          !spaces.emplace(space.id, space.limits).second)
        throw std::invalid_argument("Invalid streaming space index");
    }
    std::size_t bytes = sizeof(Impl);
    bytes = add(bytes, spaces.size() * (sizeof(SpaceDefinition) + 64));
    for (const auto &cell : manifest.cells) {
      validateDefinition(cell);
      bytes =
          add(bytes, add(sizeof(Entry) + 256,
                         add(4096, cell.dependencies.size() * sizeof(CellId))));
    }
    bytes = add(bytes, props.maxSources *
                           (sizeof(StreamingSource) + 128 +
                            manifest.cells.size() * (sizeof(CellId) + 64)));
    bytes = add(bytes, props.maxRetiring *
                           sizeof(std::weak_ptr<const detail::CellGeneration>));
    bytes = add(bytes, props.maxRequests * (sizeof(Request) + 128));
    metadata = ledger->reserve(runtime::MemoryClass::CPU,
                               runtime::ResourceKind::Streaming, bytes,
                               "Streaming metadata", {world.value, epoch, 1});
    for (auto &cell : manifest.cells) {
      const auto id = cell.id;
      if (!entries.emplace(id, Entry{.definition = std::move(cell)}).second)
        throw std::invalid_argument("Duplicate cell identity");
    }
    // Kahn traversal, deterministic CellId order; no recursive dependency
    // stack.
    std::set<CellId> resolved;
    while (order.size() != entries.size()) {
      const auto before = order.size();
      for (const auto &[id, entry] : entries) {
        if (resolved.contains(id))
          continue;
        for (auto dependency : entry.definition.dependencies)
          if (!entries.contains(dependency))
            throw std::invalid_argument("Missing cell dependency");
        if (std::all_of(entry.definition.dependencies.begin(),
                        entry.definition.dependencies.end(),
                        [&](auto dependency) {
                          return resolved.contains(dependency);
                        })) {
          order.push_back(id);
          resolved.insert(id);
        }
      }
      if (before == order.size())
        throw std::invalid_argument("Cyclic cell dependencies");
    }
    metadata->setState(runtime::AllocationState::Owned);
  }

  void validateDefinition(const CellDefinition &cell) const {
    validate(cell.id.space);
    const auto space = spaces.find(cell.id.space);
    if (space == spaces.end())
      throw std::invalid_argument("Cell has no declared space");
    cell.bounds.validate(space->second);
    if (!cell.id.value || cell.id.space.world != world ||
        cell.bounds.space != cell.id.space || !cell.revision ||
        cell.content.size() > 4096 || cell.dependencies.size() > props.maxCells)
      throw std::invalid_argument("Invalid cell definition");
    std::set<CellId> unique;
    for (auto dependency : cell.dependencies)
      if (!unique.insert(dependency).second || dependency == cell.id)
        throw std::invalid_argument("Invalid repeated cell dependency");
  }

  void active() const {
    if (closed)
      throw std::logic_error("Streaming scope is closed");
  }

  bool usable(const Entry &entry, Readiness required) const {
    return entry.current &&
           entry.current->revision == entry.definition.revision &&
           entry.current->contentGeneration == entry.contentGeneration &&
           satisfies(entry.current->product->readiness(), required);
  }

  void notify() noexcept {
    if (wake)
      try {
        wake();
      } catch (...) {
      }
  }

  void cancelJob(Entry &entry) {
    if (entry.job && entry.job->ticket && !entry.job->ticket->isCanceled()) {
      entry.job->ticket->cancel();
      ++counters.cancellations;
    }
  }

  void reconcile() {
    for (auto &[id, e] : entries) {
      e.requested = Readiness::None;
      e.priority = std::numeric_limits<int>::min();
      e.pinned = false;
    }
    for (auto &[owner, set] : sources) {
      std::set<CellId> retained;
      for (auto &[id, e] : entries)
        for (const auto &source : set.values) {
          const auto radius = set.retained.contains(id) ? source.unloadRadius
                                                        : source.loadRadius;
          const auto predicted = translated(
              source.position, source.velocity * source.predictionSeconds,
              spaces.at(source.position.space));
          if (std::min(distance(source.position, e.definition.bounds),
                       distance(predicted, e.definition.bounds)) <= radius) {
            retained.insert(id);
            e.requested = e.requested | source.required;
            e.priority = std::max(e.priority, source.priority);
          }
        }
      set.retained = std::move(retained);
    }
    for (const auto &[id, request] : requests) {
      if (request.terminal)
        continue;
      auto &e = entries.at(request.value.cell);
      if (request.value.revision != e.definition.revision)
        continue;
      e.requested = e.requested | request.value.required;
      e.priority = std::max(e.priority, request.value.priority);
      e.pinned = true;
    }
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
      auto &e = entries.at(*it);
      if (e.requested == Readiness::None)
        continue;
      for (auto dependency : e.definition.dependencies) {
        auto &d = entries.at(dependency);
        d.requested = d.requested | Readiness::Data;
        d.priority = std::max(d.priority, e.priority);
        d.pinned = d.pinned || e.pinned;
      }
    }
    for (auto &[id, e] : entries) {
      if (e.requested == Readiness::None) {
        cancelJob(e);
        if (!e.job && !e.current)
          e.status = CellStatus::Absent;
        continue;
      }
      e.retainedUntil =
          now +
          std::chrono::duration_cast<Clock::duration>(
              std::chrono::duration<double>{props.minimumRetentionSeconds});
      if (usable(e, e.requested)) {
        e.status = CellStatus::Ready;
        continue;
      }
      if (e.job && !satisfies(e.job->request.required, e.requested))
        cancelJob(e);
      if (!e.job && e.status != CellStatus::Failed) {
        if (e.status != CellStatus::Queued)
          e.queued = now;
        e.status = CellStatus::Queued;
      }
    }
    notify();
  }

  CellState state(const Entry &e) const {
    return {.status = e.status,
            .requested = e.requested,
            .available =
                e.current ? e.current->product->readiness() : Readiness::None,
            .revision = e.current ? e.current->revision : 0,
            .generation = e.generation,
            .stale = e.current && !usable(e, Readiness::None),
            .bytes = e.current ? e.current->charge->bytes() : 0,
            .leases = e.current ? std::size_t(e.current.use_count() - 1) : 0,
            .diagnostic = e.diagnostic};
  }

  void retire(Entry &e) {
    if (e.current && e.current.use_count() > 1) {
      std::erase_if(
          retired, [](const auto &generation) { return generation.expired(); });
      if (retired.size() >= props.maxRetiring)
        throw std::length_error(
            "Retained cell generations exceed retirement limit");
      retired.push_back(e.current);
      e.current->charge->setState(runtime::AllocationState::Retiring);
    }
    e.current.reset();
  }

  void close() noexcept {
    if (closed)
      return;
    closed = true;
    for (auto &[id, e] : entries) {
      cancelJob(e);
      if (e.current)
        e.current->charge->setState(runtime::AllocationState::Retiring);
      e.current.reset();
      e.job.reset(); // Worker retains its slot/reservation until actual
                     // retirement.
      e.status = CellStatus::Cancelled;
    }
    sources.clear();
    wake = {};
  }
};

WorldStreamer::WorldStreamer(WorldManifest manifest, std::uint64_t epoch,
                             std::shared_ptr<CellProvider> provider,
                             runtime::Executor &executor,
                             std::shared_ptr<runtime::ResourceLedger> ledger,
                             StreamingProps props, std::function<void()> wake)
    : _impl{std::make_shared<Impl>(std::move(manifest), epoch,
                                   std::move(provider), executor,
                                   std::move(ledger), props, std::move(wake))} {
}

WorldStreamer::~WorldStreamer() { close(); }

void WorldStreamer::setSources(std::uint64_t owner,
                               std::span<const StreamingSource> values) {
  auto &s = *_impl;
  s.active();
  if (!owner)
    throw std::invalid_argument("Streaming source owner required");
  std::size_t total = values.size();
  for (const auto &[id, sources] : s.sources)
    if (id != owner)
      total += sources.values.size();
  if (total > s.props.maxSources)
    throw std::length_error("Too many streaming sources");
  for (const auto &source : values) {
    const auto space = s.spaces.find(source.position.space);
    if (space == s.spaces.end())
      throw std::invalid_argument("Source has no declared space");
    space->second.validate(source.position);
    valid(source.required);
    if (source.position.space.world != s.world || !isFinite(source.velocity) ||
        !std::isfinite(source.loadRadius) || source.loadRadius < 0 ||
        !std::isfinite(source.unloadRadius) ||
        source.unloadRadius < source.loadRadius ||
        !std::isfinite(source.predictionSeconds) ||
        source.predictionSeconds < 0 ||
        source.predictionSeconds > s.props.maximumPredictionSeconds)
      throw std::invalid_argument("Invalid streaming source");
    translated(source.position, source.velocity * source.predictionSeconds,
               space->second);
  }
  if (values.empty())
    s.sources.erase(owner);
  else {
    auto staged = std::vector<StreamingSource>(values.begin(), values.end());
    s.sources[owner].values = std::move(staged);
  }
  s.reconcile();
}

std::uint64_t WorldStreamer::request(CellRequest request) {
  auto &s = *_impl;
  s.active();
  valid(request.required);
  auto &e = s.entries.at(request.cell);
  if (request.epoch != s.epoch || request.revision != e.definition.revision)
    throw std::invalid_argument("Stale streaming request");
  if (s.requests.size() >= s.props.maxRequests)
    throw std::length_error("Streaming request capacity exceeded");
  const auto id = next(s.sequence);
  request.generation = id;
  s.requests.emplace(id, Impl::Request{request});
  if (e.status == CellStatus::Failed || e.status == CellStatus::Cancelled)
    e.status = CellStatus::Absent;
  s.reconcile();
  return id;
}

void WorldStreamer::cancel(std::uint64_t id) {
  auto &s = *_impl;
  s.active();
  auto &request = s.requests.at(id);
  request.terminal = CellStatus::Cancelled;
  request.diagnostic = "Request cancelled";
  s.reconcile();
}

void WorldStreamer::forget(std::uint64_t id) {
  auto &s = *_impl;
  s.active();
  if (!s.requests.erase(id))
    throw std::invalid_argument("Unknown streaming request");
  s.reconcile();
}

CellState WorldStreamer::requestState(std::uint64_t id) const {
  const auto &s = *_impl;
  const auto &request = s.requests.at(id);
  auto result = s.state(s.entries.at(request.value.cell));
  if (request.terminal || s.closed) {
    result.status = s.closed ? CellStatus::Cancelled : *request.terminal;
    result.diagnostic = request.diagnostic;
  } else if (request.value.revision !=
             s.entries.at(request.value.cell).definition.revision) {
    result.status = CellStatus::Failed;
    result.diagnostic = "Requested content revision replaced";
  }
  return result;
}

CellState WorldStreamer::state(CellId id) const {
  return _impl->state(_impl->entries.at(id));
}

CellLease WorldStreamer::lease(CellId id, Readiness required,
                               std::uint64_t revision) const {
  const auto &s = *_impl;
  s.active();
  valid(required);
  const auto &e = s.entries.at(id);
  if (revision != e.definition.revision || !s.usable(e, required))
    return {};
  return CellLease{e.current};
}

void WorldStreamer::replace(CellDefinition definition) {
  auto &s = *_impl;
  s.active();
  s.validateDefinition(definition);
  auto &entry = s.entries.at(definition.id);
  if (definition.revision <= entry.definition.revision ||
      definition.dependencies != entry.definition.dependencies)
    throw std::invalid_argument(
        "Replacement requires newer revision and unchanged dependencies");
  const auto id = definition.id;
  std::set<CellId> changed{id};
  for (auto key : s.order) {
    const auto &e = s.entries.at(key);
    if (key == id ||
        std::any_of(e.definition.dependencies.begin(),
                    e.definition.dependencies.end(),
                    [&](auto dep) { return changed.contains(dep); })) {
      if (e.contentGeneration == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Streaming content generation exhausted");
      changed.insert(key);
    }
  }
  entry.definition = std::move(definition);
  for (auto key : changed) {
    auto &e = s.entries.at(key);
    ++e.contentGeneration;
    s.cancelJob(e);
    e.status = CellStatus::Absent;
  }
  s.reconcile();
}

runtime::ServiceWork WorldStreamer::advance(Clock::time_point now,
                                            runtime::ServiceWorkBudget budget) {
  return advanceImpl(*_impl, now, budget);
}

runtime::ServiceWork
WorldStreamer::advanceImpl(Impl &s, Clock::time_point now,
                           runtime::ServiceWorkBudget budget) {
  if (s.closed || !budget.operations)
    return {};
  if (!s.started) {
    s.started = true;
    for (auto &[id, e] : s.entries)
      e.queued = now;
  }
  if (now < s.now)
    throw std::invalid_argument("Streaming clock moved backwards");
  s.now = now;
  runtime::ServiceWork work;
  bool expired{};
  for (auto &[id, request] : s.requests)
    if (!request.terminal && request.value.deadline &&
        now >= *request.value.deadline &&
        !s.usable(s.entries.at(request.value.cell), request.value.required)) {
      request.terminal = CellStatus::Failed;
      request.diagnostic = "Readiness deadline exceeded";
      ++s.counters.deadlineMisses;
      expired = true;
    }
  if (expired)
    s.reconcile();
  std::erase_if(s.retired, [](const auto &entry) { return entry.expired(); });
  std::size_t jobs{};
  for (const auto &[id, e] : s.entries)
    if (e.job)
      ++jobs;
  // Stable priority aging; pinned requests precede speculative source demand.
  auto order = s.order;
  std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
    const auto &x = s.entries.at(a), &y = s.entries.at(b);
    if (bool(x.job) != bool(y.job))
      return bool(x.job);
    if (x.pinned != y.pinned)
      return x.pinned;
    const auto score = [&](const Impl::Entry &e) {
      return double(e.priority) +
             std::chrono::duration<double>(now - e.queued).count() /
                 s.props.agingSeconds;
    };
    return score(x) > score(y);
  });
  for (auto id : order) {
    if (work.operations >= budget.operations)
      break;
    auto &e = s.entries.at(id);
    if ((e.job && !e.job->ticket->retired()) ||
        (!e.job && e.requested != Readiness::None && now < e.retryAt) ||
        (!e.job &&
         ((e.requested != Readiness::None &&
           (s.usable(e, e.requested) || e.status == CellStatus::Failed)) ||
          (e.requested == Readiness::None && !e.current))))
      continue;
    ++work.operations;
    if (e.job && e.job->ticket->retired()) {
      if (work.messages >= budget.messages ||
          budget.bytes - work.bytes < sizeof(detail::CellGeneration))
        continue;
      ++work.messages;
      work.bytes += sizeof(detail::CellGeneration);
      auto slot = std::move(e.job);
      --jobs;
      const auto started = Clock::now();
      std::lock_guard lock{slot->mutex};
      s.counters.preparationSeconds += slot->seconds;
      if (slot->ticket->isCanceled() ||
          slot->contentGeneration != e.contentGeneration ||
          e.requested == Readiness::None) {
        e.status = e.requested == Readiness::None ? CellStatus::Cancelled
                                                  : CellStatus::Queued;
        e.queued = now;
        continue;
      }
      try {
        if (!slot->completed)
          throw std::runtime_error("Cell worker retired without a result");
        if (slot->error)
          std::rethrow_exception(slot->error);
        if (!slot->result ||
            !satisfies(slot->result->readiness(), slot->request.required))
          throw std::runtime_error(
              "Provider did not produce required readiness");
        valid(slot->result->readiness());
        if (slot->result->bytes() > s.props.productBytes)
          throw std::length_error("Cell product exceeds admitted output bound");
        auto generation = std::make_shared<detail::CellGeneration>();
        generation->charge = s.ledger->splitReservation(
            slot->charge,
            add(slot->result->bytes(), sizeof(detail::CellGeneration)),
            runtime::ResourceKind::Streaming);
        generation->cell = id;
        generation->revision = e.definition.revision;
        generation->contentGeneration = e.contentGeneration;
        generation->product = std::move(slot->result);
        generation->charge->setState(runtime::AllocationState::Owned);
        s.retire(e);
        e.current = std::move(generation);
        e.status = CellStatus::Ready;
        e.diagnostic.clear();
        e.retainedUntil =
            now +
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>{s.props.minimumRetentionSeconds});
        ++s.counters.published;
      } catch (const std::exception &error) {
        e.status = CellStatus::Failed;
        e.diagnostic = error.what();
        ++s.counters.failures;
      } catch (...) {
        e.status = CellStatus::Failed;
        e.diagnostic = "Unknown cell preparation failure";
        ++s.counters.failures;
      }
      s.counters.publicationSeconds +=
          std::chrono::duration<double>(Clock::now() - started).count();
    }
    if (e.requested == Readiness::None) {
      if (!e.job && now >= e.retainedUntil) {
        s.retire(e);
        e.status = CellStatus::Absent;
      }
      continue;
    }
    if (s.usable(e, e.requested)) {
      e.status = CellStatus::Ready;
      continue;
    }
    if (e.job || e.status == CellStatus::Failed || jobs >= s.props.maxJobs)
      continue;
    if (budget.bytes - work.bytes <
        sizeof(CellDefinition) + e.definition.content.size())
      continue;
    if (!satisfies(s.provider->capabilities(), e.requested)) {
      e.status = CellStatus::Failed;
      e.diagnostic = "Unsupported cell readiness/provider";
      ++s.counters.failures;
      continue;
    }
    std::vector<std::shared_ptr<const detail::CellGeneration>> dependencies;
    bool ready = true;
    for (auto id : e.definition.dependencies) {
      const auto &dep = s.entries.at(id);
      if (!s.usable(dep, Readiness::Data)) {
        ready = false;
        if (dep.status == CellStatus::Failed) {
          e.status = CellStatus::Failed;
          e.diagnostic = "Cell dependency failed";
          ++s.counters.failures;
        }
        break;
      }
      dependencies.push_back(dep.current);
    }
    if (!ready)
      continue;
    try {
      auto slot = std::make_shared<Impl::Slot>();
      const auto bytes = add(add(s.props.productBytes, s.props.scratchBytes),
                             sizeof(detail::CellGeneration));
      slot->charge = s.ledger->reserve(
          runtime::MemoryClass::CPU, runtime::ResourceKind::Preparation, bytes,
          "Cell preparation", {s.world.value, s.epoch, 1});
      slot->request = {
          id,          s.epoch,   e.definition.revision, next(e.generation),
          e.requested, e.priority};
      slot->contentGeneration = e.contentGeneration;
      slot->dependencies = std::move(dependencies);
      auto ticket = s.executor.submit(
          [slot, provider = s.provider, definition = e.definition,
           props = s.props, wake = s.wake](std::stop_token stop) noexcept {
            const auto began = Clock::now();
            std::shared_ptr<const CellProduct> result;
            std::exception_ptr error;
            try {
              std::vector<std::shared_ptr<const CellProduct>> dependencies;
              for (const auto &dep : slot->dependencies)
                dependencies.emplace_back(dep, dep->product.get());
              result = provider->prepare(definition, slot->request.required,
                                         props.productBytes, props.scratchBytes,
                                         dependencies, stop);
            } catch (...) {
              error = std::current_exception();
            }
            {
              std::lock_guard lock{slot->mutex};
              slot->result = std::move(result);
              slot->error = error;
              slot->seconds =
                  std::chrono::duration<double>(Clock::now() - began).count();
              slot->completed = true;
            }
            if (wake)
              try {
                wake();
              } catch (...) {
              }
          },
          bytes);
      if (ticket.admission == runtime::TaskAdmission::Closed ||
          ticket.admission == runtime::TaskAdmission::TooLarge)
        throw std::runtime_error(
            std::string{runtime::describe(ticket.admission)});
      if (!ticket.ticket) {
        e.status = CellStatus::Queued;
        e.retryAt = now + retryDelay;
        e.diagnostic = "Cell worker capacity unavailable";
        continue;
      }
      slot->ticket = std::move(ticket.ticket);
      e.job = std::move(slot);
      e.status = CellStatus::Preparing;
      e.diagnostic.clear();
      work.bytes += sizeof(CellDefinition) + e.definition.content.size();
      ++jobs;
    } catch (const runtime::ResourcePressure &error) {
      // Admission pressure can disappear without changing source demand.
      // Provider/content failures remain terminal until explicitly retried.
      if (error.requested > error.limit) {
        e.status = CellStatus::Failed;
        ++s.counters.failures;
      } else {
        e.status = CellStatus::Queued;
        e.retryAt = now + retryDelay;
      }
      e.diagnostic = error.what();
    } catch (const std::exception &error) {
      e.status = CellStatus::Failed;
      e.diagnostic = error.what();
      ++s.counters.failures;
    }
  }
  bool failedRequests{};
  for (auto &[id, request] : s.requests) {
    const auto &entry = s.entries.at(request.value.cell);
    if (!request.terminal && entry.status == CellStatus::Failed) {
      request.terminal = CellStatus::Failed;
      request.diagnostic = entry.diagnostic;
      failedRequests = true;
    }
  }
  if (failedRequests)
    s.reconcile();
  return work;
}

runtime::ServiceDemand WorldStreamer::demand() const {
  return demandImpl(*_impl);
}

runtime::ServiceDemand WorldStreamer::demandImpl(const Impl &s) {
  if (s.closed)
    return {};
  runtime::ServiceDemand demand;
  const auto deadline = [&](Clock::time_point time) {
    if (!demand.wakeAt || time < *demand.wakeAt)
      demand.wakeAt = time;
  };
  for (const auto &[id, e] : s.entries) {
    if (e.job) {
      demand.pending = demand.pending || e.job->ticket->retired();
      deadline(s.now + retryDelay);
    } else if (e.status == CellStatus::Queued)
      deadline(e.retryAt > s.now ? e.retryAt : s.now + retryDelay);
    if (e.requested == Readiness::None && e.current)
      deadline(e.retainedUntil);
  }
  for (const auto &[id, request] : s.requests)
    if (!request.terminal && request.value.deadline &&
        !s.usable(s.entries.at(request.value.cell), request.value.required))
      deadline(*request.value.deadline);
  return demand;
}

runtime::ServiceHandle WorldStreamer::attach(runtime::ServiceScope &scope) {
  _impl->active();
  _impl->wake = scope.wakeCallback();
  auto weak = std::weak_ptr<Impl>{_impl};
  return scope.add({.name = "world streaming",
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

StreamingStats WorldStreamer::stats() const {
  const auto &s = *_impl;
  auto stats = s.counters;
  for (const auto &[id, e] : s.entries) {
    stats.queued += e.status == CellStatus::Queued;
    stats.preparing += bool(e.job);
    stats.ready += bool(e.current);
    stats.stale += e.current && !s.usable(e, Readiness::None);
    stats.leased += e.current && e.current.use_count() > 1;
    if (e.status == CellStatus::Queued)
      stats.oldestQueueSeconds =
          std::max(stats.oldestQueueSeconds,
                   std::chrono::duration<double>(s.now - e.queued).count());
  }
  for (const auto &weak : s.retired)
    if (auto value = weak.lock())
      stats.retiringBytes = add(stats.retiringBytes, value->charge->bytes());
  return stats;
}

void WorldStreamer::close() noexcept { _impl->close(); }
} // namespace playground::world
