#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace playground::runtime {

class ActivationToken {
  std::weak_ptr<std::atomic_bool> _active;

  explicit ActivationToken(const std::shared_ptr<std::atomic_bool> &active)
      : _active{active} {}
  friend class ActivationLifetime;

public:
  ActivationToken() = default;

  bool isActive() const noexcept {
    auto active = _active.lock();
    return active && active->load(std::memory_order_acquire);
  }
};

// Activation changes and guarded callback execution share the owner thread.
// Worker-side checks are advisory; they do not grant access to owner state.
class ActivationLifetime {
  std::shared_ptr<std::atomic_bool> _active;

public:
  ActivationLifetime() = default;

  ~ActivationLifetime() { deactivate(); }

  ActivationLifetime(const ActivationLifetime &) = delete;
  ActivationLifetime &operator=(const ActivationLifetime &) = delete;

  void activate() {
    auto next = std::make_shared<std::atomic_bool>(true);
    deactivate();
    _active = std::move(next);
  }

  void deactivate() noexcept {
    if (_active)
      _active->store(false, std::memory_order_release);
  }

  ActivationToken token() const noexcept { return ActivationToken{_active}; }
};
} // namespace playground::runtime
