#pragma once

#include <exception>
#include <mutex>
#include <stop_token>
#include <string>

#include <runtime/Executor.hpp>
#include <ui/containers/Transitions.hpp>

namespace playground::ui {
enum class AsyncStatus { Pending, Ready, Error, Cancelled };

template <typename T> struct AsyncSnapshot {
  AsyncStatus status{AsyncStatus::Cancelled};
  std::uint64_t generation{};
  std::shared_ptr<const T> value;
  std::string error;
};

// Owner-thread request API. Workers only touch their request's durable slot.
template <typename T> class AsyncResource {
  struct Request {
    std::mutex mutex;
    AsyncStatus status{AsyncStatus::Pending};
    std::shared_ptr<const T> value;
    std::string error;
  };

  std::shared_ptr<Request> _request;
  std::shared_ptr<const T> _previous;
  std::optional<runtime::TaskTicket> _ticket;
  std::uint64_t _generation{};

public:
  ~AsyncResource() { cancel(); }

  AsyncResource() = default;
  AsyncResource(const AsyncResource &) = delete;
  AsyncResource &operator=(const AsyncResource &) = delete;

  template <typename Loader>
  void start(runtime::Executor &executor, Loader loader,
             std::size_t reservedBytes = 0, std::function<void()> wake = {}) {
    auto old = snapshot();
    if (old.value)
      _previous = old.value;
    cancel();
    auto request = std::make_shared<Request>();
    _request = request;
    ++_generation;
    try {
      _ticket = executor.submit(
          [request, loader = std::move(loader),
           wake = std::move(wake)](std::stop_token stop) mutable noexcept {
            try {
              auto value = std::make_shared<const T>(loader(stop));
              std::lock_guard lock{request->mutex};
              if (request->status == AsyncStatus::Pending) {
                request->status = stop.stop_requested() ? AsyncStatus::Cancelled
                                                        : AsyncStatus::Ready;
                if (request->status == AsyncStatus::Ready)
                  request->value = std::move(value);
              }
            } catch (...) {
              auto error = std::current_exception();
              std::lock_guard lock{request->mutex};
              if (request->status == AsyncStatus::Pending) {
                request->status = stop.stop_requested() ? AsyncStatus::Cancelled
                                                        : AsyncStatus::Error;
                try {
                  std::rethrow_exception(error);
                } catch (const std::exception &e) {
                  try {
                    request->error = e.what();
                  } catch (...) {
                  }
                } catch (...) {
                  try {
                    request->error = "Unknown asynchronous error";
                  } catch (...) {
                  }
                }
              }
            }
            if (wake)
              try {
                wake();
              } catch (...) {
              }
          },
          reservedBytes);
    } catch (...) {
      std::lock_guard lock{request->mutex};
      request->status = AsyncStatus::Error;
      throw;
    }
    if (!_ticket) {
      std::lock_guard lock{request->mutex};
      request->status = AsyncStatus::Error;
      request->error = "Executor admission refused";
    }
  }

  void cancel() noexcept {
    if (_ticket)
      _ticket->cancel();
    if (_request) {
      std::lock_guard lock{_request->mutex};
      if (_request->status == AsyncStatus::Pending)
        _request->status = AsyncStatus::Cancelled;
    }
  }

  AsyncSnapshot<T> snapshot() const {
    if (!_request)
      return {AsyncStatus::Cancelled, _generation, _previous, {}};
    std::lock_guard lock{_request->mutex};
    return {_request->status, _generation,
            _request->value ? _request->value : _previous, _request->error};
  }
};

template <typename T> struct AsyncViewProps {
  std::function<std::unique_ptr<Node>()> pending;
  std::function<std::unique_ptr<Node>(const T &)> ready;
  std::function<std::unique_ptr<Node>(const AsyncSnapshot<T> &)> error;
  double fallbackDelay{.15};
  bool retainPrevious{true};
};

template <typename T> class AsyncView : public TransitionHost {
  std::shared_ptr<AsyncResource<T>> _resource;
  AsyncViewProps<T> _props;
  TimerHandle _poll;
  bool _polling{};
  std::uint64_t _generation{};
  double _started{};
  bool _observed{};
  std::optional<AsyncStatus> _displayed;
  std::shared_ptr<const T> _visibleValue;

  void publish() {
    if (!services() || !services()->scheduler)
      return;
    const auto snapshot = _resource->snapshot();
    const auto now = services()->scheduler->now();
    if (!_observed || snapshot.generation != _generation) {
      _observed = true;
      _generation = snapshot.generation;
      _started = now;
      _displayed.reset();
    }
    if (snapshot.status == AsyncStatus::Pending) {
      if (_props.retainPrevious && snapshot.value) {
        if (!current() || _visibleValue != snapshot.value) {
          replace(std::to_string(_generation) + ":previous",
                  _props.ready(*snapshot.value));
          _visibleValue = snapshot.value;
        }
        _displayed = AsyncStatus::Ready;
      } else {
        if (_visibleValue) {
          replace(std::to_string(_generation) + ":waiting",
                  std::make_unique<Box>());
          _visibleValue.reset();
        }
        if (now - _started >= _props.fallbackDelay &&
            _displayed != AsyncStatus::Pending && _props.pending) {
          replace(std::to_string(_generation) + ":pending", _props.pending());
          _displayed = AsyncStatus::Pending;
        }
      }
      if (!_polling) {
        auto self = this->template handle<AsyncView>();
        _poll = services()->scheduler->schedule(
            .016,
            [self] {
              if (auto *node = self.get())
                node->publish();
            },
            .016);
        _polling = true;
      }
      return;
    }
    _poll.disconnect();
    _polling = false;
    if (_displayed == snapshot.status && snapshot.status != AsyncStatus::Ready)
      return;
    // A ready result is published once, including after retaining old success.
    const auto key = std::to_string(_generation) + ":" +
                     std::to_string(static_cast<int>(snapshot.status));
    if (key == this->key())
      return;
    std::unique_ptr<Node> content;
    if (snapshot.status == AsyncStatus::Ready && snapshot.value)
      content = _props.ready(*snapshot.value);
    else if (_props.error)
      content = _props.error(snapshot);
    if (content) {
      replace(key, std::move(content));
      _visibleValue =
          snapshot.status == AsyncStatus::Ready ? snapshot.value : nullptr;
    }
    _displayed = snapshot.status;
  }

protected:
  void onAttach(UIServices &) override {
    auto self = this->template handle<AsyncView>();
    services()->defer([self] {
      if (auto *node = self.get())
        node->refresh();
    });
  }

  void onDetach() noexcept override {
    _poll.disconnect();
    _polling = false;
    TransitionHost::onDetach();
  }

public:
  AsyncView(std::shared_ptr<AsyncResource<T>> resource, AsyncViewProps<T> props,
            layout::BoxProps box = {})
      : TransitionHost{box}, _resource{std::move(resource)},
        _props{std::move(props)} {
    if (!_resource || !_props.ready || !std::isfinite(_props.fallbackDelay) ||
        _props.fallbackDelay < 0)
      throw std::invalid_argument("Invalid async view");
  }

  // Call after starting/retrying a request on an already settled view.
  void refresh() { publish(); }

  AsyncSnapshot<T> snapshot() const { return _resource->snapshot(); }
};
} // namespace playground::ui
