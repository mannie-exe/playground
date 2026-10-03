#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <input/ViewControlSession.hpp>

namespace playground::input {
namespace {
constexpr std::array all{ViewChannel::Movement, ViewChannel::Look,
                         ViewChannel::Zoom, ViewChannel::Action};

unsigned index(ViewChannel channel) {
  const auto i = static_cast<unsigned>(channel);
  if (i >= all.size())
    throw std::invalid_argument("Invalid viewport channel");
  return i;
}

void devices(const std::vector<InputDevice> &values) {
  if (values.size() > 32)
    throw std::length_error("Too many assigned input devices");
  std::set<InputDevice> seen;
  for (auto d : values)
    if ((d.kind != DeviceKind::Keyboard && d.kind != DeviceKind::Mouse &&
         d.kind != DeviceKind::Gamepad) ||
        !seen.insert(d).second)
      throw std::invalid_argument("Invalid or duplicate assigned input device");
}
} // namespace

ViewControlSession::ViewControlSession(InputMap &input, ViewControlProps props)
    : _input{input}, _props{std::move(props)} {
  devices(_props.devices);
  if (!_props.viewport || !_props.player ||
      !std::isfinite(_props.releaseThreshold) ||
      !std::isfinite(_props.activationThreshold) ||
      _props.releaseThreshold < 0 ||
      _props.activationThreshold <= _props.releaseThreshold ||
      _props.activationThreshold > 1)
    throw std::invalid_argument("Invalid viewport control props");
  std::set<std::string> names;
  for (const auto &channel : _props.actions)
    for (const auto &name : channel)
      if (name.empty() || !names.insert(name).second)
        throw std::invalid_argument(
            "Viewport actions must belong to one channel");
}

ViewControlSession::~ViewControlSession() {
  try {
    release();
  } catch (...) {
    _pointer.reset();
  }
}

void ViewControlSession::cancel(ViewChannel channel) {
  std::vector<std::string_view> names;
  for (const auto &name : _props.actions[index(channel)])
    names.push_back(name);
  _input.cancelActions(names);
}

void ViewControlSession::assign(std::vector<InputDevice> assigned) {
  devices(assigned);
  if (assigned == _props.devices)
    return;
  suspend(ControlReason::Device);
  _props.devices = std::move(assigned);
  for (auto &channel : _state.channels)
    std::erase_if(channel.rearm, [&](auto d) {
      return std::ranges::find(_props.devices, d) == _props.devices.end();
    });
}

void ViewControlSession::setThresholds(float release, float activation) {
  if (!std::isfinite(release) || !std::isfinite(activation) || release < 0 ||
      activation <= release || activation > 1)
    throw std::invalid_argument("Invalid control takeover thresholds");
  if (release == _props.releaseThreshold &&
      activation == _props.activationThreshold)
    return;
  suspend(ControlReason::Explicit);
  _props.releaseThreshold = release;
  _props.activationThreshold = activation;
}

bool ViewControlSession::engage(const ViewEngagement &request) {
  if (!request.activation.isActive() || !request.focused || !request.eligible ||
      std::ranges::find(_props.devices, request.device) == _props.devices.end())
    return false;
  if (_state.generation == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("Viewport engagement identity exhausted");
  std::shared_ptr<void> lease;
  if (request.pointer) {
    if (request.device.kind != DeviceKind::Mouse || !request.acquirePointer)
      return false;
    try {
      lease = request.acquirePointer();
    } catch (...) {
      _state.reason = ControlReason::Acquisition;
      throw;
    }
    if (!lease) {
      _state.reason = ControlReason::Acquisition;
      return false;
    }
  }
  for (auto channel : all) {
    cancel(channel);
    _state.channels[index(channel)].enabled = true;
  }
  _activation = request.activation;
  _pointer = std::move(lease);
  _state.pointerLocked = bool(_pointer);
  _state.status = ControlStatus::Active;
  _state.reason = ControlReason::None;
  ++_state.generation;
  return true;
}

void ViewControlSession::suspend(ControlReason reason,
                                 std::span<const ViewChannel> channels) {
  for (auto channel : channels)
    index(channel);
  for (auto channel : channels) {
    auto &state = _state.channels[index(channel)];
    if (state.owner)
      state.rearm.insert(*state.owner);
    state.owner.reset();
    state.enabled = false;
    cancel(channel);
    if (channel == ViewChannel::Look) {
      _pointer.reset();
      _state.pointerLocked = false;
    }
  }
  _state.reason = reason;
  _state.status = std::ranges::any_of(_state.channels,
                                      [](const auto &c) { return c.enabled; })
                      ? ControlStatus::Active
                      : ControlStatus::Suspended;
}

void ViewControlSession::suspend(ControlReason reason) { suspend(reason, all); }

void ViewControlSession::release(std::uint64_t generation) {
  if (generation && generation != _state.generation)
    return;
  suspend(ControlReason::Explicit);
  _state.status = ControlStatus::Inactive;
  _activation = {};
}

void ViewControlSession::removeDevice(InputDevice device) {
  std::erase(_props.devices, device);
  for (auto &channel : _state.channels)
    channel.rearm.erase(device);
  for (auto channel : all)
    if (_state.channels[index(channel)].owner == device)
      suspend(ControlReason::Device, std::span{&channel, 1});
  if (device.kind == DeviceKind::Mouse && _pointer)
    suspend(ControlReason::Device, std::array{ViewChannel::Look});
}

void ViewControlSession::synchronize(bool focused, InputClaims claims) {
  if (!_activation.isActive()) {
    if (_state.status == ControlStatus::Active)
      suspend(ControlReason::App);
    return;
  }
  if (!focused) {
    suspend(ControlReason::Focus);
    return;
  }
  for (auto channel : all) {
    const auto &state = _state.channels[index(channel)];
    if (state.owner &&
        ((state.owner->kind == DeviceKind::Keyboard && claims.keyboard) ||
         (state.owner->kind == DeviceKind::Mouse &&
          (claims.pointer ||
           (!claims.capturedPointers.empty() &&
            (state.owner->id == 0 ||
             std::ranges::find(claims.capturedPointers, state.owner->id) !=
                 claims.capturedPointers.end())))) ||
         (state.owner->kind == DeviceKind::Gamepad && claims.gamepad)))
      suspend(ControlReason::UI, std::span{&channel, 1});
  }
  if ((claims.pointer || !claims.capturedPointers.empty()) && _pointer)
    suspend(ControlReason::UI,
            std::array{ViewChannel::Look, ViewChannel::Zoom});
}

bool ViewControlSession::accept(ViewChannel channel, InputDevice device,
                                float magnitude, bool intentional, bool repeat,
                                bool synthetic) {
  auto &state = _state.channels[index(channel)];
  if (!std::isfinite(magnitude) || magnitude < 0)
    throw std::invalid_argument("Invalid input magnitude");
  if (std::ranges::find(_props.devices, device) == _props.devices.end())
    return false;
  if (magnitude <= _props.releaseThreshold) {
    state.rearm.erase(device);
    return state.enabled && state.owner == device && _activation.isActive();
  }
  if (!_activation.isActive() || !state.enabled ||
      state.rearm.contains(device) || repeat || synthetic || !intentional)
    return false;
  if (state.owner == device)
    return true;
  if (magnitude < _props.activationThreshold)
    return false;
  if (state.owner) {
    const auto previous = *state.owner;
    state.rearm.insert(previous);
    std::vector<std::string_view> names;
    for (const auto &name : _props.actions[index(channel)])
      names.push_back(name);
    const auto clear = [&](ControlKind kind) {
      _input.cancelDeviceActions(names, kind,
                                 previous.kind != DeviceKind::Gamepad &&
                                         previous.id == 0
                                     ? std::nullopt
                                     : std::optional{previous.id});
    };
    if (previous.kind == DeviceKind::Keyboard)
      clear(ControlKind::Key);
    else if (previous.kind == DeviceKind::Mouse) {
      clear(ControlKind::MouseButton);
      clear(ControlKind::PointerMotion);
      clear(ControlKind::Wheel);
    } else {
      clear(ControlKind::GamepadButton);
      clear(ControlKind::GamepadAxis);
    }
  }
  state.owner = device;
  return true;
}
} // namespace playground::input
