#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

#include <runtime/Activity.hpp>
#include <runtime/CompletionQueue.hpp>
#include <support/Test.hpp>
#include <ui/RuntimeServices.hpp>

using namespace playground;

int main() {
  return test::run([] {
    runtime::ActivityProps props;
    test::require(props.continuousPaint && props.continuousUpdate,
                  "legacy cadence remains explicit default");
    runtime::PaintRequest paint;
    const auto first = paint.capture();
    test::require(paint.pending(), "new presentation begins dirty");
    paint.request();
    paint.submitted(first);
    test::require(paint.pending(),
                  "in-frame request survives successful older frame");
    paint.submitted(paint.capture());
    test::require(!paint.pending(), "acknowledged frame goes idle");
    paint.request();
    test::require(paint.pending(),
                  "skipped and failed frame cannot consume request");

    const auto now = runtime::ActivityClock::now();
    runtime::ActivityDemand demand{.wakeAt = now + std::chrono::seconds{1}};
    test::require(!demand.updateDue(now) &&
                      demand.updateDue(now + std::chrono::seconds{1}),
                  "deadline boundary is inclusive");
    ui::Scheduler scheduler;
    test::require(!scheduler.nextDelay(), "empty scheduler has no deadline");
    int ticks{};
    auto repeat = scheduler.schedule(.5, [&] { ++ticks; }, .5);
    auto canceled = scheduler.schedule(.1, [] {});
    canceled.disconnect();
    test::require(scheduler.nextDelay() == .5,
                  "canceled timer cannot hold earliest wake");
    scheduler.advance(.25);
    test::require(scheduler.nextDelay() == .25 && ticks == 0,
                  "remaining scheduler delay");
    scheduler.advance(10);
    test::require(ticks == 1 && scheduler.nextDelay() == .5,
                  "idle catch-up does not repeat unboundedly");
    repeat.disconnect();
    test::require(!scheduler.nextDelay(), "last cancellation removes deadline");

    runtime::CompletionQueue queue;
    std::atomic<int> wakes{};
    queue.setWakeCallback([&] {
      test::require(
          queue.pending() > 0,
          "work is published before notification, without queue lock");
      ++wakes;
    });
    int completed{};
    std::jthread worker{[&] { queue.sink().post([&] { ++completed; }); }};
    worker.join();
    test::require(wakes == 1 && completed == 0,
                  "worker wakes owner without running completion");
    queue.drain();
    queue.setWakeCallback(
        [] { throw std::runtime_error("notification failed"); });
    test::require(queue.sink().post([&] { ++completed; }) &&
                      queue.pending() == 1,
                  "failed wake preserves accepted work for fallback poll");
    queue.drain();
    test::require(completed == 2, "fallback drain delivers accepted work");

    struct PostOnDestruction {
      runtime::CompletionSink sink;
      int &completed;

      ~PostOnDestruction() {
        sink.post([&completed = completed] { ++completed; });
      }
    };
    auto capture = std::make_shared<PostOnDestruction>(queue.sink(), completed);
    queue.setWakeCallback([capture] {});
    capture.reset();
    queue.setWakeCallback({});
    test::require(queue.pending() == 1,
                  "replaced wake captures can reenter the completion queue");
    queue.drain();
    test::require(completed == 3, "retired callback posts are delivered");

    queue.close();
    test::require(!queue.sink().post([] {}), "closed owner rejects late work");
  });
}
