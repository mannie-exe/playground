#pragma once

#include <cstdint>
#include <optional>

namespace playground::runtime {

struct SimulationTimingProps {
  double stepSeconds{1.0 / 60.0};
  double maxFrameSeconds{0.25};
  unsigned maxStepsPerFrame{8};
  void validate() const;
};

struct SimulationStep {
  std::uint64_t tick;
  double seconds;
};

struct SimulationState {
  std::uint64_t tick{};
  double alpha{};
  double droppedSeconds{};
  bool paused{};
};

class SimulationClock {
  SimulationTimingProps _props;

  SimulationState _state;
  double _accumulated{};
  unsigned _remaining{};

public:
  explicit SimulationClock(SimulationTimingProps props = {});

  const SimulationTimingProps &props() const noexcept { return _props; }

  SimulationState state() const noexcept;
  void beginFrame(double elapsedSeconds);
  std::optional<SimulationStep> nextStep();
  void setPaused(bool) noexcept;
  void reset() noexcept;
};
} // namespace playground::runtime
