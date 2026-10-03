#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <support/TaskGate.hpp>
#include <support/Test.hpp>
#include <world/Streaming.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

namespace {
struct Bytes final : CellProduct {
  std::vector<std::byte> data{std::byte{42}};

  Readiness readiness() const noexcept override { return Readiness::Data; }

  std::size_t bytes() const noexcept override { return data.size(); }
};

struct Provider final : CellProvider {
  std::atomic<unsigned> calls{};
  std::shared_ptr<test::TaskGate> gate;
  std::promise<void> entered;

  Readiness capabilities() const noexcept override { return Readiness::Data; }

  std::shared_ptr<const CellProduct>
  prepare(const CellDefinition &definition, Readiness, std::size_t, std::size_t,
          std::span<const std::shared_ptr<const CellProduct>> dependencies,
          std::stop_token) override {
    ++calls;
    if (definition.content == "failed")
      throw std::runtime_error("fixture read failed");
    if (definition.content == "gate") {
      entered.set_value();
      gate->wait(
          {}); // Deliberately ignores cancellation until actual retirement.
    }
    if (!definition.dependencies.empty())
      require(!dependencies.empty(), "dependency products retained");
    return std::make_shared<Bytes>();
  }
};
} // namespace

int main() {
  return test::run([] {
    const SpaceId space{{71}, 1};
    const CellId a{space, 1}, b{space, 2}, c{space, 3};
    const auto cell = [&](CellId id, double low, double high) {
      return CellDefinition{
          id, {space, {low, -1, -1}, {high, 1, 1}}, 1, "local"};
    };
    auto ca = cell(a, -10, 0), cb = cell(b, 0, 10), cc = cell(c, 10, 20);
    cc.dependencies = {b};
    auto ledger = std::make_shared<runtime::ResourceLedger>();
    runtime::Executor executor;
    auto provider = std::make_shared<Provider>();
    WorldStreamer stream{
        {space.world, 1, "fixture-lock", {ca, cb, cc}, {{space, {}}}},
        9,
        provider,
        executor,
        ledger,
        {.productBytes = 1024,
         .scratchBytes = 1024,
         .minimumRetentionSeconds = 0}};
    auto now = runtime::ActivityClock::time_point{};
    const auto pump = [&](auto done) {
      const auto timeout =
          std::chrono::steady_clock::now() + std::chrono::seconds{5};
      do {
        const auto used = stream.advance(now, {4, 4096, 4});
        require(used.operations <= 4 && used.bytes <= 4096 &&
                    used.messages <= 4,
                "streaming respects each boundary grant");
        if (done())
          return;
        std::this_thread::yield();
      } while (std::chrono::steady_clock::now() < timeout);
      throw std::runtime_error("streaming fixture did not settle");
    };
    const std::array first{StreamingSource{{space, {-5, 0, 0}}, {}, 0, 3}};
    stream.setSources(1, first);
    pump([&] { return stream.state(a).status == CellStatus::Ready; });
    auto leaseA = stream.lease(a, Readiness::Data, 1);
    require(leaseA && leaseA.cell() == a && leaseA.revision() == 1,
            "negative authored cell loads and yields retained product");
    require(stream.state(b).status == CellStatus::Absent,
            "unrequested neighbors stay absent");
    stream.setSources(
        1, std::array{StreamingSource{{space, {2, 0, 0}}, {}, 0, 3}});
    pump([&] { return stream.state(b).status == CellStatus::Ready; });
    require(bool(stream.lease(a, Readiness::Data, 1)),
            "unload radius retains a cell after leaving its load radius");
    stream.setSources(2, first);
    stream.setSources(
        1, std::array{StreamingSource{{space, {5, 0, 0}}, {}, 0, 3}});
    pump([&] { return stream.state(b).status == CellStatus::Ready; });
    require(bool(stream.lease(a, Readiness::Data, 1)),
            "another owner's demand preserves the cell");
    stream.setSources(2, {});
    pump([&] { return stream.state(a).status == CellStatus::Absent; });
    require(leaseA.product().bytes() == 1 && stream.stats().retiringBytes > 0,
            "discovery retirement preserves consumer leases and accounting");
    leaseA = {};
    stream.advance(now);
    require(stream.stats().retiringBytes == 0,
            "final lease release retires old bytes");
    auto oldB = stream.lease(b, Readiness::Data, 1);
    cb.revision = 2;
    cb.content = "failed";
    stream.replace(cb);
    pump([&] { return stream.state(b).status == CellStatus::Failed; });
    require(stream.state(b).stale && stream.state(b).revision == 1 && oldB,
            "failed replacement retains explicit stale previous generation");
    require(!stream.lease(b, Readiness::Data, 2),
            "stale generation cannot satisfy current readiness");
    const auto failedCalls = provider->calls.load();
    stream.advance(now);
    require(provider->calls == failedCalls &&
                stream.state(b).status == CellStatus::Failed,
            "content errors do not trigger automatic retry loops");
    cb.revision = 3;
    cb.content = "local";
    stream.replace(cb);
    pump([&] { return stream.state(b).status == CellStatus::Ready; });
    require(bool(stream.lease(b, Readiness::Data, 3)),
            "new revision repairs failed preparation");
    stream.setSources(1, {});
    const auto request = stream.request({c, 9, 1});
    pump([&] {
      return stream.requestState(request).status == CellStatus::Ready;
    });
    auto leaseC = stream.lease(c, Readiness::Data, 1);
    require(leaseC && stream.lease(b, Readiness::Data, 3),
            "explicit request prepares dependency closure");
    stream.cancel(request);
    require(stream.requestState(request).status == CellStatus::Cancelled,
            "cancelled terminal result survives notification pressure");
    stream.forget(request);
    const auto unsupported = stream.request({a, 9, 1, 0, Readiness::Query});
    pump([&] {
      return stream.requestState(unsupported).status == CellStatus::Failed;
    });
    require(stream.state(a).diagnostic.find("Unsupported") != std::string::npos,
            "missing query provider never claims successful readiness");
    stream.forget(unsupported);
    const auto deadline = stream.request({a, 9, 1, 0, Readiness::Data, 0, now});
    stream.advance(now);
    require(stream.requestState(deadline).status == CellStatus::Failed &&
                stream.stats().deadlineMisses == 1,
            "expired unmet requests fail instead of retrying forever");
    test::rejects([&] { stream.request({a, 8, 1}); }, "foreign epoch rejected");
    auto invalid = first;
    invalid[0].unloadRadius = -1;
    test::rejects([&] { stream.setSources(1, invalid); },
                  "invalid source rejects before publication");
    {
      auto cycleA = ca, cycleB = cb;
      cycleA.dependencies = {b};
      cycleB.dependencies = {a};
      test::rejects(
          [&] {
            WorldStreamer bad{
                {space.world, 1, "", {cycleA, cycleB}, {{space, {}}}},
                9,
                provider,
                executor,
                ledger};
          },
          "dependency cycles rejected without recursion");
    }
    stream.close();
    require(!stream.demand().pending && !stream.demand().wakeAt && leaseC,
            "closed streamer releases demand while leases remain usable");
    {
      auto account = std::make_shared<runtime::ResourceLedger>();
      auto source = std::make_shared<Provider>();
      WorldStreamer pressured{{space.world, 1, "", {ca, cb}, {{space, {}}}},
                              12,
                              source,
                              executor,
                              account,
                              {.productBytes = 1024, .scratchBytes = 1024}};
      auto held = account->reserve(runtime::MemoryClass::CPU,
                                   runtime::ResourceKind::Preparation,
                                   account->snapshot().budgets.preparationBytes,
                                   "Fixture preparation pressure");
      pressured.setSources(1, first);
      const auto expiring =
          pressured.request({b, 12, cb.revision, 0, Readiness::Data, 0,
                             now + std::chrono::milliseconds{5}});
      pressured.advance(now);
      require(pressured.state(a).status == CellStatus::Queued &&
                  !pressured.state(a).diagnostic.empty() && !source->calls,
              "temporary admission pressure retains queued source demand");
      require(pressured.demand().wakeAt && *pressured.demand().wakeAt > now,
              "pressure schedules a bounded retry without a busy wake");
      const auto refusals = account->snapshot().refusals;
      held.reset();
      pressured.advance(now);
      require(account->snapshot().refusals == refusals && !source->calls,
              "pressure retry waits for its service deadline");
      pressured.advance(now + std::chrono::milliseconds{5});
      require(
          pressured.requestState(expiring).status == CellStatus::Failed &&
              pressured.state(a).status == CellStatus::Queued && !source->calls,
          "explicit deadlines expire while source demand waits for capacity");
      auto retryAt = *pressured.demand().wakeAt;
      const auto timeout =
          std::chrono::steady_clock::now() + std::chrono::seconds{5};
      while (pressured.state(a).status != CellStatus::Ready &&
             std::chrono::steady_clock::now() < timeout) {
        pressured.advance(retryAt);
        std::this_thread::yield();
      }
      require(pressured.state(a).status == CellStatus::Ready &&
                  source->calls == 1 && pressured.state(a).diagnostic.empty(),
              "stationary source recovers when preparation capacity returns");
    }
    {
      runtime::ServicePump pump;
      auto scope = pump.scope();
      WorldStreamer scoped{{space.world, 1, "", {ca}, {{space, {}}}},
                           11,
                           provider,
                           executor,
                           ledger};
      auto handle = scoped.attach(scope);
      scoped.request({a, 11, 1});
      require(scoped.advance(now, {0, 0, 0}).operations == 0 &&
                  scoped.state(a).status == CellStatus::Queued,
              "zero grant does not start preparation");
      scope.close();
      require(handle.status() == runtime::ServiceStatus::Closed &&
                  !scoped.demand().pending && !scoped.demand().wakeAt,
              "scope close invalidates streaming publication and demand");
      test::rejects<std::logic_error>([&] { scoped.request({a, 11, 1}); },
                                      "closed scope refuses new cell work");
    }
    {
      auto blocked = std::make_shared<Provider>();
      blocked->gate = std::make_shared<test::TaskGate>();
      auto entered = blocked->entered.get_future();
      auto gated = ca;
      gated.content = "gate";
      auto isolated = std::make_shared<runtime::ResourceLedger>();
      runtime::Executor worker;
      struct ReleaseGate {
        std::shared_ptr<test::TaskGate> gate;
        ~ReleaseGate() { gate->open(); }
      } release{blocked->gate};
      WorldStreamer pending{{space.world, 1, "", {gated}, {{space, {}}}},
                            10,
                            blocked,
                            worker,
                            isolated,
                            {.productBytes = 1024, .scratchBytes = 1024}};
      auto id = pending.request({a, 10, 1});
      pending.advance(now);
      require(entered.wait_for(std::chrono::seconds{5}) ==
                  std::future_status::ready,
              "controlled worker entered preparation");
      const auto before = isolated->snapshot().memory[0].bytes;
      pending.cancel(id);
      pending.close();
      require(isolated->snapshot().memory[0].bytes == before,
              "cancelled running jobs remain charged until actual retirement");
      blocked->gate->open();
    }
  });
}
