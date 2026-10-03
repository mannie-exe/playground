#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <runtime/ActivationLifetime.hpp>
#include <runtime/Activity.hpp>

namespace playground::runtime {
namespace detail {
struct ServicePumpState;
struct ServiceEntry;
} // namespace detail

struct ServiceDemand {
  bool pending{};
  std::optional<ActivityClock::time_point> wakeAt;

  bool due(ActivityClock::time_point now) const noexcept {
    return pending || (wakeAt && *wakeAt <= now);
  }
};

struct ServiceWorkBudget {
  std::size_t operations{64}, bytes{1024 * 1024}, messages{64};
  void validate() const;
};

struct ServiceWork {
  std::size_t operations{}, bytes{}, messages{};
};

struct ServiceRegistration {
  std::string name;
  std::function<ServiceDemand()> demand;
  std::function<ServiceWork(ActivityClock::time_point, ServiceWorkBudget)>
      advance;
  std::function<void()> cancel;
  ServiceWorkBudget budget;
};

enum class ServiceStatus { Active, Closed, Failed };

struct ServiceFailure {
  std::uint64_t id{};
  std::string name;
  std::exception_ptr error;
};

// Status retention does not extend callback or app lifetime. Owner-thread only.
class ServiceHandle {
  std::shared_ptr<detail::ServiceEntry> _entry;
  explicit ServiceHandle(std::shared_ptr<detail::ServiceEntry>);
  friend class ServiceScope;

public:
  std::uint64_t id() const noexcept;
  ServiceStatus status() const noexcept;
  std::exception_ptr failure() const noexcept;
  void close() noexcept;
};

class ServiceScope {
  std::weak_ptr<detail::ServicePumpState> _state;
  std::uint64_t _id{};
  std::unique_ptr<ActivationLifetime> _lifetime;
  ServiceScope(std::shared_ptr<detail::ServicePumpState>, std::uint64_t);
  friend class ServicePump;

public:
  ServiceScope() = default;
  ~ServiceScope();
  ServiceScope(const ServiceScope &) = delete;
  ServiceScope &operator=(const ServiceScope &) = delete;
  ServiceScope(ServiceScope &&) noexcept;
  ServiceScope &operator=(ServiceScope &&) noexcept;

  ServiceHandle add(ServiceRegistration);
  // Closing invalidates dispatch immediately; in-flight callback cleanup waits
  // until traversal returns. Callbacks cannot destroy their own domain owner.
  void close() noexcept;
  bool isOpen() const noexcept;
  // Copy to workers; services retain their own synchronized result storage.
  std::function<void()> wakeCallback() const;
};

struct ServicePumpProps {
  std::size_t maxServices{256}, maxVisits{64};
  ServiceWorkBudget budget;
  void validate() const;
};

struct ServicePumpStats {
  std::uint64_t visits{}, failures{};
  ServiceWork consumed;
};

class ServicePump {
  std::shared_ptr<detail::ServicePumpState> _state;

public:
  explicit ServicePump(ServicePumpProps = {});
  ~ServicePump();
  ServicePump(const ServicePump &) = delete;
  ServicePump &operator=(const ServicePump &) = delete;
  ServiceScope scope();
  void setWakeCallback(std::function<void()>);
  ServiceDemand demand();
  ServiceWork advance(ActivityClock::time_point);
  std::vector<ServiceFailure> takeFailures();
  ServicePumpStats stats() const;
  void close() noexcept;
};

} // namespace playground::runtime
