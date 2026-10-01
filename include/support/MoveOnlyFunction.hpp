#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace playground::support {

#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
template <class Signature>
using MoveOnlyFunction = std::move_only_function<Signature>;
#else
namespace detail {
// Apple libc++ does not yet provide C++23 move_only_function. The fallback
// covers the unqualified, optionally noexcept signatures used by our APIs.
template <class R, bool Noexcept, class... Args> class MoveOnlyFunction {
  struct Target {
    virtual ~Target() = default;
    virtual R invoke(Args &&...args) noexcept(Noexcept) = 0;
  };

  template <class F> struct Callable final : Target {
    F function;

    template <class T>
    explicit Callable(T &&value) : function(std::forward<T>(value)) {}

    R invoke(Args &&...args) noexcept(Noexcept) override {
      if constexpr (std::is_void_v<R>)
        std::invoke(function, std::forward<Args>(args)...);
      else
        return std::invoke(function, std::forward<Args>(args)...);
    }
  };

  std::unique_ptr<Target> _target;

public:
  MoveOnlyFunction() noexcept = default;
  MoveOnlyFunction(std::nullptr_t) noexcept {}
  MoveOnlyFunction(MoveOnlyFunction &&) noexcept = default;
  MoveOnlyFunction &operator=(MoveOnlyFunction &&) noexcept = default;
  MoveOnlyFunction(const MoveOnlyFunction &) = delete;
  MoveOnlyFunction &operator=(const MoveOnlyFunction &) = delete;

  template <class F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, MoveOnlyFunction> &&
             std::is_invocable_r_v<R, std::decay_t<F> &, Args...> &&
             std::is_invocable_r_v<R, std::decay_t<F>, Args...> &&
             (!Noexcept ||
              (std::is_nothrow_invocable_r_v<R, std::decay_t<F> &, Args...> &&
               std::is_nothrow_invocable_r_v<R, std::decay_t<F>, Args...>)) &&
             std::is_constructible_v<std::decay_t<F>, F>)
  MoveOnlyFunction(F &&function) {
    if constexpr (std::is_pointer_v<std::decay_t<F>> ||
                  std::is_member_pointer_v<std::decay_t<F>>) {
      if (function == nullptr)
        return;
    }
    _target = std::make_unique<Callable<std::decay_t<F>>>(
        std::forward<F>(function));
  }

  MoveOnlyFunction &operator=(std::nullptr_t) noexcept {
    _target.reset();
    return *this;
  }

  explicit operator bool() const noexcept { return bool(_target); }

  R operator()(Args... args) noexcept(Noexcept) {
    assert(_target);
    return _target->invoke(std::forward<Args>(args)...);
  }

  void swap(MoveOnlyFunction &other) noexcept { _target.swap(other._target); }

  friend void swap(MoveOnlyFunction &a, MoveOnlyFunction &b) noexcept {
    a.swap(b);
  }

  friend bool operator==(const MoveOnlyFunction &function,
                         std::nullptr_t) noexcept {
    return !function;
  }
};

template <class Signature> struct MoveOnlyFunctionType;
template <class R, class... Args> struct MoveOnlyFunctionType<R(Args...)> {
  using type = MoveOnlyFunction<R, false, Args...>;
};
template <class R, class... Args>
struct MoveOnlyFunctionType<R(Args...) noexcept> {
  using type = MoveOnlyFunction<R, true, Args...>;
};
} // namespace detail

template <class Signature>
using MoveOnlyFunction = typename detail::MoveOnlyFunctionType<Signature>::type;
#endif

} // namespace playground::support
