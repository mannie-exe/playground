#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <runtime/SimulationClock.hpp>

namespace playground::runtime {
void SimulationTimingProps::validate() const {
  if (!std::isfinite(stepSeconds) || stepSeconds <= 0 ||
      !std::isfinite(maxFrameSeconds) || maxFrameSeconds < stepSeconds ||
      maxFrameSeconds > std::numeric_limits<double>::max() / 2 ||
      !maxStepsPerFrame || maxFrameSeconds / stepSeconds > 1e9)
    throw std::invalid_argument("Invalid fixed-step timing policy");
}

SimulationClock::SimulationClock(SimulationTimingProps props) : _props{props} {
  props.validate();
}

void SimulationClock::beginFrame(double elapsed) {
  if (!std::isfinite(elapsed) || elapsed < 0)
    throw std::invalid_argument("Invalid simulation elapsed time");
  if (_remaining)
    throw std::logic_error("Finish fixed ticks before beginning another frame");
  if (_state.paused)
    return;
  const double accepted = std::min(elapsed, _props.maxFrameSeconds);
  _state.droppedSeconds = std::min(std::numeric_limits<double>::max(),
                                   _state.droppedSeconds + elapsed - accepted);
  _accumulated += accepted;
  const double due = std::floor(_accumulated / _props.stepSeconds);
  _remaining = static_cast<unsigned>(
      std::min(due, static_cast<double>(_props.maxStepsPerFrame)));
  if (due > _remaining) {
    const double dropped = (due - _remaining) * _props.stepSeconds;
    _state.droppedSeconds = std::min(std::numeric_limits<double>::max(),
                                     _state.droppedSeconds + dropped);
    _accumulated -= dropped;
  }
}

std::optional<SimulationStep> SimulationClock::nextStep() {
  if (!_remaining)
    return {};
  if (_state.tick == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Simulation tick exhausted");
  --_remaining;
  _accumulated = std::max(0.0, _accumulated - _props.stepSeconds);
  return SimulationStep{++_state.tick, _props.stepSeconds};
}

SimulationState SimulationClock::state() const noexcept {
  auto result = _state;
  result.alpha = std::clamp(_accumulated / _props.stepSeconds, 0.0, 1.0);
  return result;
}

void SimulationClock::setPaused(bool paused) noexcept {
  if (_state.paused == paused)
    return;
  _state.paused = paused;
  const double fraction = std::fmod(_accumulated, _props.stepSeconds);
  _state.droppedSeconds =
      std::min(std::numeric_limits<double>::max(),
               _state.droppedSeconds + (_accumulated - fraction));
  _accumulated = fraction;
  _remaining = 0;
}

void SimulationClock::reset() noexcept {
  _state = {};
  _remaining = 0;
  _accumulated = 0;
}
} // namespace playground::runtime
