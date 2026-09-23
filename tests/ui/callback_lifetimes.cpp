#include <support/Test.hpp>
#include <ui/RuntimeServices.hpp>

#include <memory>
#include <stdexcept>

int main() {
  return playground::test::run([] {
    using namespace playground;
    ui::Scheduler scheduler;
    ui::Signal<> signal;

    auto owner = std::make_shared<int>(42);
    std::weak_ptr<int> observed = owner;
    auto timer = scheduler.schedule(1000, [owner] {});
    owner.reset();
    timer.disconnect();
    test::require(observed.expired(), "cancel releases delayed timer captures");

    owner = std::make_shared<int>(42);
    observed = owner;
    auto subscription = signal.connect([owner] {});
    owner.reset();
    subscription.disconnect();
    test::require(observed.expired(), "disconnect releases signal captures");

    owner = std::make_shared<int>(42);
    observed = owner;
    timer = scheduler.schedule(
        0,
        [&, owner] {
          timer.disconnect();
          test::require(
              *owner == 42 && !observed.expired(),
              "self-cancellation must not destroy the executing callable");
          throw std::runtime_error("callback failure");
        },
        1);
    owner.reset();
    test::rejects<std::runtime_error>([&] { scheduler.advance(0); },
                                      "cancelled timer exception propagates");
    test::require(observed.expired(),
                  "throwing cancelled timer releases captures");
    scheduler.advance(1);

    owner = std::make_shared<int>(42);
    observed = owner;
    int depth{};
    subscription = signal.connect([&, owner] {
      if (++depth == 1)
        signal.emit();
      else
        subscription.disconnect();
      test::require(
          *owner == 42 && !observed.expired(),
          "nested self-disconnect retains captures until all calls exit");
    });
    owner.reset();
    signal.emit();
    signal.emit();
    test::require(depth == 2 && observed.expired(),
                  "disconnected recursive slot is not called again");

    int calls{};
    auto repeating = scheduler.schedule(
        0,
        [&] {
          if (++calls == 1)
            throw std::runtime_error("retry next tick");
        },
        1);
    test::rejects<std::runtime_error>([&] { scheduler.advance(0); },
                                      "repeating timer exception propagates");
    scheduler.advance(1);
    test::require(calls == 2,
                  "failure does not silently cancel a repeating timer");
  });
}
