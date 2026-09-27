#pragma once

#include <stdexcept>
#include <utility>
#include <variant>

namespace playground {

struct Keep {
  bool operator==(const Keep &) const = default;
};

struct Reset {
  bool operator==(const Reset &) const = default;
};

// An update operation, not a nullable value. Set(nullopt) and Reset differ.
template <typename T> class Patch {
  std::variant<Keep, T, Reset> _operation;

  explicit Patch(std::variant<Keep, T, Reset> operation)
      : _operation(std::move(operation)) {}

public:
  Patch() = default;

  static Patch keep() { return {}; }

  static Patch set(T value) {
    return Patch{
        std::variant<Keep, T, Reset>{std::in_place_index<1>, std::move(value)}};
  }

  static Patch reset() {
    return Patch{std::variant<Keep, T, Reset>{std::in_place_index<2>}};
  }

  bool isKeep() const noexcept { return _operation.index() == 0; }

  bool isSet() const noexcept { return _operation.index() == 1; }

  bool isReset() const noexcept { return _operation.index() == 2; }

  const T *value() const noexcept { return std::get_if<1>(&_operation); }

  template <typename Visitor> decltype(auto) visit(Visitor &&visitor) const {
    return std::visit(std::forward<Visitor>(visitor), _operation);
  }

  T appliedTo(const T &current, const T &baseline) const {
    if (isSet())
      return *value();
    return isReset() ? baseline : current;
  }

  // Required values without a documented baseline cannot be reset.
  T appliedTo(const T &current) const {
    if (isReset())
      throw std::invalid_argument("Cannot reset a required property");
    return isSet() ? *value() : current;
  }

  bool operator==(const Patch &) const = default;
};

} // namespace playground
