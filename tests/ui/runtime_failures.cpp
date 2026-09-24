#include <limits>
#include <vector>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Box.hpp>

using namespace playground;

int main() {
  return test::run([] {
    ui::Signal<> signal;
    int first{}, second{}, late{};
    ui::Connection secondToken, lateToken;
    auto firstToken = signal.connect([&] {
      ++first;
      secondToken.disconnect();
      if (first == 1)
        lateToken = signal.connect([&] { ++late; });
    });
    secondToken = signal.connect([&] { ++second; });
    signal.emit();
    test::require(first == 1 && second == 0 && late == 0,
                  "disconnect affects current emission, additions wait");
    signal.emit();
    test::require(first == 2 && late == 1,
                  "new subscription participates next emission");
    test::rejects([&] { signal.connect({}); }, "empty subscription rejected");
    auto throwing =
        signal.connect([] { throw std::runtime_error("test callback"); });
    test::rejects<std::runtime_error>([&] { signal.emit(); },
                                      "signal reports callback failure");
    throwing.disconnect();
    signal.emit();
    test::require(first == 4 && late == 3,
                  "signal remains usable after exception");

    ui::Scheduler scheduler;
    std::vector<int> order;
    std::vector<ui::TimerHandle> timers;
    timers.push_back(scheduler.schedule(0, [&] {
      order.push_back(1);
      timers.push_back(scheduler.schedule(0, [&] { order.push_back(3); }));
    }));
    timers.push_back(scheduler.schedule(0, [&] { order.push_back(2); }));
    scheduler.advance(0);
    test::require(order == std::vector{1, 2},
                  "equal deadlines FIFO; new timers deferred");
    scheduler.advance(0);
    test::require(order == std::vector{1, 2, 3},
                  "new timer executes next advance");
    auto failure = scheduler.schedule(
        0, [] { throw std::runtime_error("timer failure"); });
    test::rejects<std::runtime_error>([&] { scheduler.advance(0); },
                                      "timer exception propagates");
    scheduler.advance(0); // Advancing guard must have unwound.
    auto recursive = scheduler.schedule(0, [&] { scheduler.advance(0); });
    test::rejects<std::logic_error>([&] { scheduler.advance(0); },
                                    "recursive advance rejected");
    test::rejects([&] { scheduler.advance(-1); }, "negative time rejected");
    test::rejects(
        [&] {
          scheduler.schedule(0, [] {}, std::numeric_limits<double>::infinity());
        },
        "infinite interval rejected");
    test::rejects([&] { scheduler.schedule(0, {}); },
                  "empty timer callback rejected");
    scheduler.advance(0);

    ui::UIRoot root;
    root.setContent(std::make_unique<ui::Box>());
    root.flushLayout({100, 100});
    root.flushChanges();
    auto *node = root.content();
    auto subscriber = node->onChanged([](const ui::ChangeSet &) {
      throw std::runtime_error("notification failure");
    });
    node->setBoxProps({.padding = math::Insets::all(5)});
    test::rejects<std::runtime_error>([&] { root.flushChanges(); },
                                      "notification exception is observable");
    test::require(node->boxProps().padding == math::Insets::all(5),
                  "committed props not rolled back by notification");
    subscriber.disconnect();
    root.flushLayout({100, 100});
    test::require(node->layoutResult().contentBounds ==
                      math::rect(5, 5, 90, 90),
                  "layout recovers after failed notification");
    bool continued{};
    root.defer(
        [](ui::UIRoot &) { throw std::runtime_error("command failure"); });
    root.defer([&](ui::UIRoot &) { continued = true; });
    test::rejects<std::runtime_error>([&] { root.flushMutations(); },
                                      "mutation failure propagates");
    root.flushMutations();
    test::require(continued,
                  "later queued commands survive a throwing command");
  });
}
