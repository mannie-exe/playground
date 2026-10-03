#include <chrono>
#include <memory>
#include <stdexcept>
#include <vector>

#include <runtime/Services.hpp>
#include <support/Test.hpp>

using namespace playground;
using namespace playground::runtime;
using namespace std::chrono_literals;
using test::require;

int main() {
  return test::run([] {
    const ActivityClock::time_point start{};
    ServicePump pump{{.maxServices = 4, .maxVisits = 1}};
    auto scope = pump.scope();
    std::vector<int> order;
    auto registration = [&](int index) {
      return ServiceRegistration{
          .name = "count",
          .demand = [] { return ServiceDemand{.pending = true}; },
          .advance =
              [&, index](auto, auto budget) {
                require(budget.operations > 0, "positive work grant");
                order.push_back(index);
                return ServiceWork{1, 2, 1};
              }};
    };
    const auto a = scope.add(registration(1));
    const auto b = scope.add(registration(2));
    const auto c = scope.add(registration(3));
    require(a.id() != b.id() && b.id() != c.id(),
            "service identities are unique");
    for (int n = 0; n < 6; ++n)
      pump.advance(start + n * 1ms);
    require(order == std::vector<int>{1, 2, 3, 1, 2, 3},
            "visit budget rotates fairly across pending services");
    require(pump.stats().consumed.operations == 6 &&
                pump.stats().consumed.bytes == 12 && pump.demand().pending,
            "unfinished demand and work totals persist");
    test::rejects([&] { pump.advance(start); },
                  "backward service clock rejected");
    scope.close();
    scope.close();
    require(a.status() == ServiceStatus::Closed && !scope.isOpen() &&
                !pump.demand().pending,
            "scope closure removes demand and invalidates retained handles");
    test::rejects<std::logic_error>([&] { scope.add(registration(4)); },
                                    "closed scope rejects registration");

    ServicePump timed;
    auto timedScope = timed.scope();
    int calls{}, cancellations{}, wakes{};
    bool done{};
    timed.setWakeCallback([&] { ++wakes; });
    const auto deadline = start + 100ms;
    const auto timer = timedScope.add(
        {.name = "timer",
         .demand =
             [&] {
               return done ? ServiceDemand{}
                           : ServiceDemand{.wakeAt = deadline};
             },
         .advance =
             [&](auto now, auto) {
               require(now >= deadline, "timer never runs early");
               ++calls;
               done = true;
               return ServiceWork{1};
             },
         .cancel = [&] { ++cancellations; }});
    require(timed.demand().wakeAt == deadline &&
                timed.demand().wakeAt == deadline,
            "demand query preserves the original absolute deadline");
    timed.advance(start);
    require(calls == 0, "idle timers do not execute");
    const auto wake = timedScope.wakeCallback();
    wake();
    require(wakes == 1,
            "worker wake endpoint is independent of model execution");
    timed.advance(deadline);
    timed.advance(deadline + 1ms);
    require(calls == 1 && !timed.demand().wakeAt,
            "completed timer stops scheduling");
    auto moved = std::move(timedScope);
    require(!timedScope.isOpen() && moved.isOpen(),
            "scope ownership moves without cancellation");
    moved.close();
    wake();
    require(wakes == 1, "closed scope suppresses late worker wakes");
    require(cancellations == 1 && timer.status() == ServiceStatus::Closed,
            "cancellation executes once before owner teardown");

    ServicePump errors;
    auto failingScope = errors.scope();
    int survivors{};
    const auto failing =
        failingScope.add({.name = "bad demand",
                          .demand = []() -> ServiceDemand {
                            throw std::runtime_error("demand error");
                          },
                          .advance = [](auto, auto) { return ServiceWork{}; }});
    failingScope.add({.name = "survivor",
                      .demand = [] { return ServiceDemand{true}; },
                      .advance =
                          [&](auto, auto) {
                            ++survivors;
                            return ServiceWork{1};
                          }});
    require(errors.demand().pending &&
                failing.status() == ServiceStatus::Failed,
            "demand failure disables only its service and requests reporting");
    errors.advance(start);
    require(survivors == 1 && failing.failure(),
            "peer service progresses after another fails");
    const auto failures = errors.takeFailures();
    require(failures.size() == 1 && failures[0].id == failing.id() &&
                errors.takeFailures().empty(),
            "failure delivered once while handle retains terminal error");
    test::rejects<std::runtime_error>(
        [&] { std::rethrow_exception(failing.failure()); },
        "original service error retained");
    const auto excess = failingScope.add(
        {.name = "over budget",
         .demand = [] { return ServiceDemand{true}; },
         .advance = [](auto,
                       auto b) { return ServiceWork{b.operations + 1}; }});
    errors.advance(start + 1ms);
    require(excess.status() == ServiceStatus::Failed &&
                errors.takeFailures().size() == 1,
            "budget violation cannot silently become successful service work");
    const auto recursive =
        failingScope.add({.name = "recursive",
                          .demand = [] { return ServiceDemand{true}; },
                          .advance =
                              [&](auto now, auto) {
                                errors.advance(now);
                                return ServiceWork{};
                              }});
    errors.advance(start + 2ms);
    require(recursive.status() == ServiceStatus::Failed,
            "recursive pumping is rejected and contained");
    errors.takeFailures();
    int individuallyClosed{};
    auto individual =
        failingScope.add({.name = "individual",
                          .demand = [] { return ServiceDemand{}; },
                          .advance = [](auto, auto) { return ServiceWork{}; },
                          .cancel = [&] { ++individuallyClosed; }});
    individual.close();
    individual.close();
    require(individuallyClosed == 1 && failingScope.isOpen(),
            "individual close preserves peer scope and cancels once");

    ServicePump during;
    auto duringScope = during.scope();
    bool returned{}, cleaned{};
    const auto closing = duringScope.add(
        {.name = "close in callback",
         .demand = [] { return ServiceDemand{true}; },
         .advance =
             [&](auto, auto) {
               duringScope.close();
               require(!cleaned, "running callback storage retained");
               returned = true;
               return ServiceWork{1};
             },
         .cancel =
             [&] {
               require(returned, "cleanup runs after dispatch returns");
               cleaned = true;
             }});
    during.advance(start);
    require(cleaned && closing.status() == ServiceStatus::Closed,
            "deferred removal settles before advance returns");

    ServicePump cascade;
    auto early = cascade.scope(), late = cascade.scope();
    int cleanedCount{};
    early.add({.name = "early",
               .demand = [] { return ServiceDemand{}; },
               .advance = [](auto, auto) { return ServiceWork{}; },
               .cancel = [&] { ++cleanedCount; }});
    late.add({.name = "late",
              .demand = [] { return ServiceDemand{}; },
              .advance = [](auto, auto) { return ServiceWork{}; },
              .cancel =
                  [&] {
                    early.close();
                    ++cleanedCount;
                  }});
    late.close();
    require(cleanedCount == 2,
            "cascading scope closure completes before domains can die");

    ServicePump bounded{{.maxServices = 1}};
    auto limited = bounded.scope();
    limited.add(registration(7));
    test::rejects<std::length_error>([&] { limited.add(registration(8)); },
                                     "registration count is bounded");
    int pumpWakes{};
    bounded.setWakeCallback([&] { ++pumpWakes; });
    const auto pumpWake = limited.wakeCallback();
    bounded.close();
    pumpWake();
    require(pumpWakes == 0, "closed pump suppresses retained wake callbacks");
    require(!limited.isOpen(), "pump closure closes all scopes");
    test::rejects<std::logic_error>([&] { bounded.scope(); },
                                    "closed pump refuses new scopes");
    test::rejects([&] { ServicePump invalid{{.maxVisits = 0}}; },
                  "empty visit budget rejected");
  });
}
