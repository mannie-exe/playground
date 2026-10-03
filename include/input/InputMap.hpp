#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <math/Geometry2D.hpp>

namespace playground::input {

enum class ControlKind {
  Key,
  MouseButton,
  GamepadButton,
  GamepadAxis,
  PointerMotion,
  Wheel
};
enum class ActionKind { Button, Axis, Vector, Delta };
enum class DeltaCadence { Frame, Tick };
enum class InputStage { BeforeUI, AfterUI };

struct Control {
  ControlKind kind{};
  int code{};
  std::uint32_t device{};
  auto operator<=>(const Control &) const = default;
};

struct InputEvent {
  Control control;
  float value{};
  bool repeat{};
  math::Vec2f displacement{};
};

// Persistent UI reservation, independent of whether an event performed work.
// BeforeUI contexts are explicit global overrides and do not obey these claims.
struct InputClaims {
  bool keyboard{}, pointer{}, gamepad{};
  std::vector<std::uint64_t> capturedPointers;

  bool owns(const Control &) const noexcept;
  bool operator==(const InputClaims &) const = default;
};

struct Binding {
  std::string action;
  ActionKind kind{ActionKind::Button};
  ControlKind control{ControlKind::Key};
  int code{};
  std::optional<std::uint32_t> device;
  math::Vec2f contribution{1, 0};
  float deadZone{};
  DeltaCadence cadence{DeltaCadence::Frame};
};

struct InputContextProps {
  std::string name;
  int priority{};
  InputStage stage{InputStage::AfterUI};
  bool consume{true};
  bool enabled{true};
};

struct ActionState {
  ActionKind kind{};
  bool held{}, pressed{}, released{}, canceled{};
  math::Vec2f value{};
};

struct InputSnapshot {
  std::map<std::string, ActionState, std::less<>> actions;
  const ActionState &operator[](std::string_view name) const noexcept;
};

using ContextId = std::uint64_t;

// Owner-thread input routing. Bindings describe physical controls, never text.
class InputMap {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  InputMap();
  ~InputMap();
  InputMap(const InputMap &) = delete;
  InputMap &operator=(const InputMap &) = delete;

  ContextId addContext(InputContextProps, std::vector<Binding>);
  InputContextProps contextProps(ContextId) const;
  std::vector<Binding> bindings(ContextId) const;
  void setContextProps(ContextId, InputContextProps);
  void removeContext(ContextId);
  void setEnabled(ContextId, bool);
  void rebind(ContextId, std::vector<Binding>);
  bool setUIClaims(InputClaims);
  const InputClaims &uiClaims() const noexcept;
  float physicalValue(Control) const noexcept;
  float physicalValue(ControlKind, int code) const noexcept;
  // Call BeforeUI once, then AfterUI with the accumulated consumption result.
  bool route(const InputEvent &, InputStage, bool blocked = false);
  void cancelAll();
  void cancelActions(std::span<const std::string_view>);
  void cancelDeviceActions(std::span<const std::string_view>, ControlKind,
                           std::optional<std::uint32_t>);
  void cancelDevice(ControlKind, std::uint32_t);
  void inheritHeld(const InputMap &);
  InputSnapshot takeFrameSnapshot();
  InputSnapshot takeTickSnapshot();
};

template <class RouteUI>
bool routeInputEvent(InputMap &map, const InputEvent &event, bool blocked,
                     RouteUI &&routeUI) {
  const bool reserved = map.uiClaims().owns(event.control);
  blocked = map.route(event, InputStage::BeforeUI, blocked);
  if (!blocked)
    blocked = routeUI();
  return map.route(event, InputStage::AfterUI, blocked || reserved);
}
} // namespace playground::input
