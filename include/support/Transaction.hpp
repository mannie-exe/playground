#pragma once

#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>

namespace playground {

class RestorationFailure : public std::runtime_error {
  std::exception_ptr _operation, _restoration;

public:
  RestorationFailure(std::exception_ptr operation,
                     std::exception_ptr restoration)
      : std::runtime_error{
            "Operation failed and previous runtime could not be restored"},
        _operation{std::move(operation)}, _restoration{std::move(restoration)} {
  }
  const std::exception_ptr &operation() const noexcept { return _operation; }
  const std::exception_ptr &restoration() const noexcept {
    return _restoration;
  }
};

// Rollback applies only to state explicitly captured by the caller. External
// effects and already-submitted native work cannot be undone by this helper.
template <class Apply, class Restore>
void withRestoration(Apply &&apply, Restore &&restore) {
  try {
    std::invoke(std::forward<Apply>(apply));
  } catch (...) {
    auto original = std::current_exception();
    try {
      std::invoke(std::forward<Restore>(restore));
    } catch (...) {
      throw RestorationFailure{std::move(original), std::current_exception()};
    }
    std::rethrow_exception(original);
  }
}

} // namespace playground
