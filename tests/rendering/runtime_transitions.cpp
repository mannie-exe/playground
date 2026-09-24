#include <rendering/RenderFailure.hpp>
#include <runtime/UpdateClock.hpp>
#include <support/Test.hpp>
#include <support/Transaction.hpp>

using namespace playground;

int main() {
  return test::run([] {
    runtime::UpdateClock clock;
    test::require(clock.advance(100, 1000) == 0,
                  "first update establishes a clock baseline");
    test::require(clock.advance(350, 1000) == 0.25,
                  "update delta uses monotonic counter frequency");
    clock.rebase();
    test::require(clock.advance(100000, 1000) == 0,
                  "recovery pause is excluded from next simulation update");
    test::require(clock.advance(100250, 1000) == 0.25,
                  "normal update clock resumes after rebase");
    test::rejects([&] { clock.advance(0, 1000); },
                  "backwards clock rejected without publishing it");
    test::rejects([&] { clock.advance(100500, 0); },
                  "zero clock frequency rejected");

    int published = 1;
    int restored{};
    withRestoration([&] { published = 2; }, [&] { ++restored; });
    test::require(published == 2 && restored == 0,
                  "successful activation needs no restoration");
    test::rejects<std::logic_error>(
        [&] {
          withRestoration(
              [&] {
                published = 3;
                throw std::logic_error{"activation failed"};
              },
              [&] {
                published = 2;
                ++restored;
              });
        },
        "original activation failure is preserved");
    test::require(published == 2 && restored == 1,
                  "failed activation restores published state");
    bool retainedBoth{};
    try {
      withRestoration([] { throw std::logic_error{"original"}; },
                      [] { throw std::runtime_error{"restore"}; });
    } catch (const RestorationFailure &failure) {
      test::rejects<std::logic_error>(
          [&] { std::rethrow_exception(failure.operation()); },
          "operation retained");
      test::rejects<std::runtime_error>(
          [&] { std::rethrow_exception(failure.restoration()); },
          "restoration retained");
      retainedBoth = true;
    }
    test::require(retainedBoth, "combined failure is explicit");

    rendering::RecoveryState recovery;
    test::require(recovery.begin("submit failed"), "first recovery allowed");
    recovery.recovered();
    recovery.observeCompleted(1, 0.1);
    test::require(
        !recovery.begin("failed again"),
        "one successful completed frame does not reset attempt budget");
    test::require(recovery.status() == rendering::RecoveryStatus::Exhausted,
                  "exhaustion observable");
    recovery.observeCompleted(2, 100);
    test::require(recovery.status() == rendering::RecoveryStatus::Exhausted,
                  "completion cannot silently reopen an exhausted budget");
    recovery = rendering::RecoveryState{};
    test::require(recovery.begin("another explicitly authorized attempt"),
                  "explicit new recovery state has a fresh budget");
    recovery.recovered();
    recovery.observeCompleted(1, 100);
    test::require(recovery.healthySeconds() == 0.25,
                  "one stalled completion cannot satisfy healthy probation");
    recovery.observeCompleted(1, 1);
    test::require(
        recovery.healthySeconds() == 0.25,
        "submission without new completed work does not advance health");
    recovery.skipped();
    test::require(recovery.healthySeconds() == 0,
                  "skipped presentation interrupts healthy probation");
    for (std::uint64_t sequence = 2; sequence <= 21; ++sequence)
      recovery.observeCompleted(sequence, 0.25);
    test::require(recovery.status() == rendering::RecoveryStatus::Ready &&
                      recovery.attempts() == 0,
                  "five seconds of advancing completed work resets the budget");
    test::require(recovery.begin("later failure"),
                  "healthy probation permits recovery from a later failure");
    recovery.failed();
    test::require(recovery.status() == rendering::RecoveryStatus::Exhausted,
                  "recreate failure observable");
    rendering::RecoveryState disabled{{.maximumAttempts = 0}};
    test::require(!disabled.begin("failure"), "recovery can be disabled");
    test::rejects([] { rendering::RecoveryState{{.healthySeconds = 0}}; },
                  "invalid probation policy rejected");
    test::rejects([&] { recovery.observeCompleted(0, -1); },
                  "invalid elapsed interval rejected");
  });
}
