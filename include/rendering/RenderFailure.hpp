#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace playground::rendering {

// Native acquisition/submission failures are recoverable candidates, not proof
// of device loss. Validation, allocation and application exceptions stay
// distinct.
class RenderFailure : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct RecoveryPolicy {
  unsigned maximumAttempts{1};
  double healthySeconds{5.0};
  double maximumProgressSeconds{0.25};

  void validate() const {
    if (!std::isfinite(healthySeconds) || healthySeconds <= 0 ||
        !std::isfinite(maximumProgressSeconds) || maximumProgressSeconds <= 0)
      throw std::invalid_argument(
          "Recovery probation intervals must be positive");
  }
};

enum class RecoveryStatus { Ready, Recovering, Recovered, Exhausted };

class RecoveryState {
  RecoveryPolicy _policy;
  unsigned _attempts{};
  RecoveryStatus _status{RecoveryStatus::Ready};
  std::string _reason;
  double _healthySeconds{};
  std::uint64_t _completedWork{};

public:
  explicit RecoveryState(RecoveryPolicy policy = {}) : _policy{policy} {
    _policy.validate();
  }
  bool begin(std::string reason) {
    _reason = std::move(reason);
    if (_status == RecoveryStatus::Exhausted ||
        _status == RecoveryStatus::Recovering ||
        _attempts >= _policy.maximumAttempts) {
      _status = RecoveryStatus::Exhausted;
      return false;
    }
    ++_attempts;
    _healthySeconds = 0;
    _completedWork = 0;
    _status = RecoveryStatus::Recovering;
    return true;
  }
  void recovered() noexcept {
    if (_status == RecoveryStatus::Recovering)
      _status = RecoveryStatus::Recovered;
  }
  void failed() noexcept { _status = RecoveryStatus::Exhausted; }
  void observeCompleted(std::uint64_t sequence, double elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
      throw std::invalid_argument("Recovery progress interval is invalid");
    if (_status != RecoveryStatus::Recovered)
      return;
    if (sequence < _completedWork) {
      // A replacement backend starts a new completion sequence.
      _healthySeconds = 0;
      _completedWork = sequence;
      return;
    }
    if (sequence == _completedWork)
      return;
    _completedWork = sequence;
    _healthySeconds += std::min(elapsedSeconds, _policy.maximumProgressSeconds);
    if (_healthySeconds >= _policy.healthySeconds) {
      _attempts = 0;
      _status = RecoveryStatus::Ready;
    }
  }
  void skipped() noexcept { _healthySeconds = 0; }
  RecoveryStatus status() const noexcept { return _status; }
  unsigned attempts() const noexcept { return _attempts; }
  double healthySeconds() const noexcept { return _healthySeconds; }
  const RecoveryPolicy &policy() const noexcept { return _policy; }
  const std::string &reason() const noexcept { return _reason; }
};

} // namespace playground::rendering
