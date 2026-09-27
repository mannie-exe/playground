#include <cmath>
#include <limits>
#include <stdexcept>

#include <runtime/SimulationClock.hpp>
#include <support/Test.hpp>

using namespace playground::runtime;
using playground::test::require;

int main() {
  return playground::test::run([] {
    SimulationClock clock{
        {.stepSeconds = 0.01, .maxFrameSeconds = 0.25, .maxStepsPerFrame = 4}};
    clock.beginFrame(0.005);
    require(!clock.nextStep() && std::abs(clock.state().alpha - 0.5) < 1e-9,
            "fractional frame");
    clock.beginFrame(0.02);
    auto first = clock.nextStep();
    auto second = clock.nextStep();
    require(first && second && first->tick == 1 && second->tick == 2 &&
                first->seconds == 0.01,
            "monotonic fixed steps");
    require(!clock.nextStep() && std::abs(clock.state().alpha - 0.5) < 1e-9,
            "remainder retained");
    clock.beginFrame(1);
    unsigned count{};
    while (clock.nextStep())
      ++count;
    require(count == 4 && std::abs(clock.state().droppedSeconds - 0.96) < 1e-9,
            "frame cap plus bounded catchup reports dropped time");
    const auto frozenAlpha = clock.state().alpha;
    clock.setPaused(true);
    clock.beginFrame(100);
    require(!clock.nextStep() && clock.state().alpha == frozenAlpha,
            "paused time not accumulated");
    clock.setPaused(false);
    clock.beginFrame(0);
    require(!clock.nextStep(), "resume has no backlog");
    bool rejected{};
    try {
      clock.beginFrame(std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "invalid elapsed rejected");
    rejected = false;
    try {
      SimulationClock bad{{.stepSeconds = 0}};
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected, "zero timestep rejected");
    clock.reset();
    clock.beginFrame(0.035);
    clock.nextStep();
    rejected = false;
    try {
      clock.beginFrame(0);
    } catch (const std::logic_error &) {
      rejected = true;
    }
    require(rejected, "unfinished tick batch cannot be overwritten");
    clock.setPaused(true);
    require(!clock.nextStep() && std::abs(clock.state().alpha - 0.5) < 1e-9 &&
                std::abs(clock.state().droppedSeconds - 0.02) < 1e-9,
            "pause inside batch drops whole ticks but preserves presentation "
            "fraction");
    clock.reset();
    double elapsed{};
    for (int i = 0; i < 1000; ++i) {
      const double dt = (i % 7) * 0.001;
      elapsed += dt;
      clock.beginFrame(dt);
      while (clock.nextStep()) {
      }
      require(clock.state().alpha >= 0 && clock.state().alpha < 1,
              "bounded interpolation");
      const double accounted = clock.state().tick * 0.01 +
                               clock.state().alpha * 0.01 +
                               clock.state().droppedSeconds;
      require(std::abs(accounted - elapsed) < 1e-10, "time conservation");
    }
  });
}
