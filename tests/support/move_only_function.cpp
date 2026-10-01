#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <support/MoveOnlyFunction.hpp>
#include <support/Test.hpp>

using playground::support::MoveOnlyFunction;
namespace test = playground::test;

namespace {
struct LvalueOnly {
  void operator()() & {}
};

struct ThrowingCopy {
  ThrowingCopy() = default;
  ThrowingCopy(const ThrowingCopy &) { throw std::runtime_error("copy"); }
  void operator()() {}
};

struct Receiver {
  int value{};
  int add(int amount) noexcept { return value += amount; }
};

struct CallbackOwner {
  explicit CallbackOwner(MoveOnlyFunction<void() noexcept>) {}
  CallbackOwner(CallbackOwner &&) noexcept = default;
  CallbackOwner(const CallbackOwner &) = delete;
};

static_assert(!std::is_copy_constructible_v<MoveOnlyFunction<void()>>);
static_assert(!std::is_copy_assignable_v<MoveOnlyFunction<void()>>);
static_assert(std::is_nothrow_move_constructible_v<MoveOnlyFunction<void()>>);
static_assert(std::is_nothrow_move_assignable_v<MoveOnlyFunction<void()>>);
static_assert(!std::is_constructible_v<MoveOnlyFunction<void()>, LvalueOnly>);
static_assert(!std::is_constructible_v<MoveOnlyFunction<void() noexcept>,
                                       decltype([] {})>);
static_assert(std::is_constructible_v<MoveOnlyFunction<void() noexcept>,
                                      decltype([]() noexcept {})>);
static_assert(noexcept(std::declval<MoveOnlyFunction<void() noexcept> &>()()));
static_assert(!noexcept(std::declval<MoveOnlyFunction<void()> &>()()));
static_assert(std::is_nothrow_move_constructible_v<CallbackOwner>);
static_assert(!std::is_constructible_v<MoveOnlyFunction<void() noexcept>,
                                       CallbackOwner>);
} // namespace

int main() {
  return test::run([] {
    MoveOnlyFunction<int(int)> empty;
    test::require(!empty && empty == nullptr, "default callback is empty");
    int (*nullFunction)(int){};
    MoveOnlyFunction<int(int)> null{nullFunction};
    test::require(!null, "null function pointer is empty");

    auto lifetime = std::make_shared<int>(7);
    std::weak_ptr<int> weak = lifetime;
    MoveOnlyFunction<int(int)> callback{
        [owned = std::make_unique<int>(3), lifetime](int amount) mutable {
          *owned += amount;
          return *owned + *lifetime;
        }};
    lifetime.reset();
    test::require(callback(2) == 12 && callback(1) == 13,
                  "move-only mutable captures retain state");
    auto moved = std::move(callback);
    test::require(!callback && moved(1) == 14,
                  "move transfers callback and empties source");
    empty = std::move(moved);
    test::require(!moved && empty(0) == 14, "move assignment transfers state");
    swap(empty, null);
    test::require(!empty && null(0) == 14, "swap preserves capture ownership");
    null = nullptr;
    test::require(!null && weak.expired(), "reset releases captures");

    MoveOnlyFunction<int(std::unique_ptr<int>, int &)> forward{
        [](std::unique_ptr<int> value, int &out) { return out = *value; }};
    int result{};
    test::require(forward(std::make_unique<int>(42), result) == 42 &&
                      result == 42,
                  "move-only arguments and references are forwarded");
    MoveOnlyFunction<void()> discard{[] { return 42; }};
    discard();
    MoveOnlyFunction<void()> throwing{
        [] { throw std::runtime_error("callback"); }};
    test::rejects<std::runtime_error>([&] { throwing(); },
                                      "throwing callbacks propagate errors");

    Receiver receiver;
    MoveOnlyFunction<int(Receiver &, int) noexcept> member{&Receiver::add};
    test::require(member(receiver, 5) == 5, "member callbacks preserve noexcept");
    int (Receiver::*nullMember)(int) noexcept = nullptr;
    MoveOnlyFunction<int(Receiver &, int) noexcept> noMember{nullMember};
    test::require(!noMember, "null member pointer is empty");

    bool retained{};
    MoveOnlyFunction<void()> original{[&] { retained = true; }};
    ThrowingCopy copy;
    test::rejects<std::runtime_error>([&] { original = copy; },
                                      "failed callable construction propagates");
    original();
    test::require(retained, "failed assignment preserves previous callback");
  });
}
