#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>

#include <input/InputMap.hpp>
#include <runtime/ActivationLifetime.hpp>

namespace playground::input {
enum class DeviceKind { Keyboard, Mouse, Gamepad };

struct InputDevice {
  DeviceKind kind{};
  std::uint32_t id{}; // Zero groups local keyboards/mice; gamepads use explicit
                      // instance IDs.
  auto operator<=>(const InputDevice &) const = default;
};
enum class ViewChannel { Movement, Look, Zoom, Action, Count };
enum class ControlStatus { Inactive, Active, Suspended };
enum class ControlReason {
  None,
  Explicit,
  Focus,
  UI,
  Device,
  App,
  Target,
  Cinematic,
  Acquisition
};

struct ChannelState {
  bool enabled{};
  std::optional<InputDevice> owner;
  std::set<InputDevice> rearm;
};

struct ViewControlState {
  ControlStatus status{ControlStatus::Inactive};
  ControlReason reason{ControlReason::None};
  std::uint64_t generation{};
  bool pointerLocked{};
  std::array<ChannelState, static_cast<unsigned>(ViewChannel::Count)> channels;
};

struct ViewControlProps {
  std::uint64_t viewport{1}, player{1};
  float releaseThreshold{.15f}, activationThreshold{.25f};
  std::vector<InputDevice> devices{{DeviceKind::Keyboard, 0},
                                   {DeviceKind::Mouse, 0}};
  std::array<std::vector<std::string>,
             static_cast<unsigned>(ViewChannel::Count)>
      actions;
};

struct ViewEngagement {
  runtime::ActivationToken activation;
  InputDevice device;
  bool focused{}, eligible{}, pointer{};
  // Adapter owns the native lease through its destructor; returning null is
  // failure.
  std::function<std::shared_ptr<void>()> acquirePointer;
};

// Owner-thread ownership policy. InputMap remains the sole physical event
// router.
class ViewControlSession {
  InputMap &_input;
  ViewControlProps _props;
  ViewControlState _state;
  runtime::ActivationToken _activation;
  std::shared_ptr<void> _pointer;
  void cancel(ViewChannel);

public:
  ViewControlSession(InputMap &, ViewControlProps = {});
  ~ViewControlSession();
  ViewControlSession(const ViewControlSession &) = delete;
  ViewControlSession &operator=(const ViewControlSession &) = delete;

  const ViewControlState &state() const noexcept { return _state; }

  const ViewControlProps &props() const noexcept { return _props; }

  void assign(std::vector<InputDevice>);
  void setThresholds(float release, float activation);
  bool engage(const ViewEngagement &);
  void suspend(ControlReason, std::span<const ViewChannel>);
  void suspend(ControlReason);
  void release(std::uint64_t generation = 0);
  void removeDevice(InputDevice);
  void synchronize(bool focused, InputClaims);
  // Raw magnitude drives neutral/takeover hysteresis; returned acceptance does
  // not consume the InputMap snapshot.
  bool accept(ViewChannel, InputDevice, float magnitude,
              bool intentional = true, bool repeat = false,
              bool synthetic = false);
};
} // namespace playground::input
