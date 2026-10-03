#include <atomic>
#include <future>
#include <memory>

#include <runtime/Executor.hpp>
#include <support/TaskGate.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    test::rejects([] { runtime::Executor executor{{.workers = 0}}; },
                  "zero workers rejected");
    runtime::Executor executor{
        {.workers = 1, .maxOutstanding = 2, .maxReservedBytes = 32}};
    auto gate = std::make_shared<test::TaskGate>();
    std::promise<void> entered;
    auto started = entered.get_future();
    auto first = executor.submit(
        [gate, &entered](std::stop_token stop) noexcept {
          entered.set_value();
          gate->wait(stop);
        },
        16);
    test::require(first.ticket.has_value(), "first task admitted");
    started.get();
    std::atomic<int> executions{};
    auto second =
        executor.submit([&](std::stop_token) noexcept { ++executions; }, 16);
    test::require(second.ticket.has_value(), "queued task admitted");
    test::require(executor.stats().outstanding == 2 &&
                      executor.stats().reservedBytes == 32 &&
                      executor.stats().maxReservedBytes == 32 &&
                      !executor.stats().closed,
                  "reservations include active and pending jobs");
    test::require(
        executor.submit([](std::stop_token) noexcept {}, 1).admission ==
            runtime::TaskAdmission::Busy,
        "count/budget backpressure");
    test::require(
        executor.submit([](std::stop_token) noexcept {}, 33).admission ==
            runtime::TaskAdmission::TooLarge,
        "oversized work is permanent even while capacity is busy");
    second.ticket->cancel();
    test::require(executor.stats().reservedBytes == 32,
                  "cancellation retains admission until worker retirement");
    executor.close();
    test::require(first.ticket->isCanceled() && second.ticket->isCanceled(),
                  "close cancels active and queued work");
    test::require(second.ticket->retired(),
                  "discarded queued work reports retirement");
    test::require(
        executor.submit([](std::stop_token) noexcept {}, 0).admission ==
            runtime::TaskAdmission::Closed,
        "closed rejects publication");
    test::require(executor.stats().closed,
                  "shutdown is observable separately from capacity pressure");
    test::require(executions == 0, "canceled queued task never runs");
    test::rejects([&] { executor.submit({}, 0); }, "empty task rejected");
    std::atomic<bool> stopped{};
    {
      runtime::Executor joining;
      std::promise<void> running;
      auto ready = running.get_future();
      auto hold = std::make_shared<test::TaskGate>();
      joining.submit(
          [&running, &stopped, hold](std::stop_token stop) noexcept {
            running.set_value();
            hold->wait(stop);
            stopped = stop.stop_requested();
          },
          0);
      ready.get();
    }
    test::require(stopped, "destruction requests stop and joins active work");
    auto concurrentGate = std::make_shared<test::TaskGate>();
    runtime::Executor concurrent{{.workers = 2}};
    std::promise<void> startedA, startedB;
    auto readyA = startedA.get_future(), readyB = startedB.get_future();
    auto a = concurrent.submit(
        [&startedA, concurrentGate](std::stop_token stop) noexcept {
          startedA.set_value();
          concurrentGate->wait(stop);
        },
        0);
    auto b = concurrent.submit(
        [&startedB, concurrentGate](std::stop_token stop) noexcept {
          startedB.set_value();
          concurrentGate->wait(stop);
        },
        0);
    readyA.get();
    readyB.get();
    test::require(a.ticket && b.ticket && concurrent.stats().outstanding == 2,
                  "two independent jobs can execute concurrently");
    concurrentGate->open();
  });
}
