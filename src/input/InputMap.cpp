#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

#include <input/InputMap.hpp>

namespace playground::input {
const ActionState &
InputSnapshot::operator[](std::string_view name) const noexcept {
  static const ActionState empty;
  const auto it = actions.find(name);
  return it == actions.end() ? empty : it->second;
}

struct InputMap::Impl {
  struct Bound {
    Binding props;
    std::map<Control, float> values;
    std::set<Control> suppressed;
  };

  struct Context {
    ContextId id;
    InputContextProps props;
    std::vector<Bound> bindings;
  };

  std::vector<Context> contexts;
  std::map<Control, float> physical;
  InputSnapshot frame, tick;
  ContextId next{1};

  Context &find(ContextId id) {
    auto it = std::ranges::find(contexts, id, &Context::id);
    if (it == contexts.end())
      throw std::out_of_range("Unknown input context");
    return *it;
  }

  static bool matches(const Binding &b, const Control &c) {
    return b.control == c.kind && b.code == c.code &&
           (!b.device || *b.device == c.device);
  }

  std::vector<Bound> prepare(std::vector<Binding> bindings,
                             ContextId replacing = 0) {
    std::map<std::string, ActionKind> kinds;
    for (const auto &context : contexts)
      if (context.id != replacing)
        for (const auto &b : context.bindings)
          kinds.emplace(b.props.action, b.props.kind);
    std::vector<Bound> result;
    for (auto &b : bindings) {
      if (b.action.empty() || b.code < 0 || !std::isfinite(b.deadZone) ||
          b.deadZone < 0 || b.deadZone >= 1 ||
          !std::isfinite(b.contribution.x) ||
          !std::isfinite(b.contribution.y) ||
          (b.kind != ActionKind::Button && b.kind != ActionKind::Axis &&
           b.kind != ActionKind::Vector) ||
          (b.control != ControlKind::Key &&
           b.control != ControlKind::MouseButton &&
           b.control != ControlKind::GamepadButton &&
           b.control != ControlKind::GamepadAxis))
        throw std::invalid_argument("Invalid action binding");
      if (auto [it, added] = kinds.emplace(b.action, b.kind);
          !added && it->second != b.kind)
        throw std::invalid_argument("Action name has incompatible kinds");
      Bound bound{std::move(b), {}, {}};
      for (const auto &[control, value] : physical)
        if (value != 0 && matches(bound.props, control))
          bound.suppressed.insert(control);
      result.push_back(std::move(bound));
    }
    return result;
  }

  void publish(bool cancel = false) {
    InputSnapshot current;
    std::map<std::string, std::pair<double, double>> sums;
    for (const auto &c : contexts)
      for (const auto &b : c.bindings) {
        auto &a = current.actions[b.props.action];
        a.kind = b.props.kind;
        if (!c.props.enabled)
          continue;
        for (const auto &[control, value] : b.values) {
          a.held |= value != 0;
          auto &sum = sums[b.props.action];
          sum.first += static_cast<double>(value) * b.props.contribution.x;
          sum.second += static_cast<double>(value) * b.props.contribution.y;
        }
      }
    for (auto &[name, a] : current.actions) {
      auto [x, y] = sums[name];
      if (a.kind == ActionKind::Button)
        a.value = {a.held ? 1.0f : 0.0f, 0};
      else if (a.kind == ActionKind::Axis)
        a.value = {static_cast<float>(std::clamp(x, -1.0, 1.0)), 0};
      else {
        const double divisor = std::max(1.0, std::hypot(x, y));
        a.value = {static_cast<float>(x / divisor),
                   static_cast<float>(y / divisor)};
      }
      if (a.kind != ActionKind::Button)
        a.held = a.value.x != 0 || a.value.y != 0;
    }
    for (auto *snapshot : {&frame, &tick}) {
      for (const auto &[name, a] : current.actions)
        snapshot->actions.try_emplace(name, ActionState{.kind = a.kind});
      for (auto &[name, old] : snapshot->actions) {
        const auto &now = current[name];
        if (current.actions.contains(name))
          old.kind = now.kind;
        if (cancel && (old.held != now.held || old.value != now.value)) {
          old.pressed = old.released = false;
          old.canceled = true;
        } else {
          old.pressed |= !old.held && now.held;
          old.released |= old.held && !now.held;
        }
        old.held = now.held;
        old.value = now.value;
      }
      std::erase_if(snapshot->actions, [&](const auto &entry) {
        const auto &state = entry.second;
        return !current.actions.contains(entry.first) && !state.pressed &&
               !state.released && !state.canceled;
      });
    }
  }

  void suppress(Context &c) {
    for (auto &b : c.bindings) {
      b.values.clear();
      for (const auto &[control, value] : physical)
        if (value != 0 && matches(b.props, control))
          b.suppressed.insert(control);
    }
  }

  static InputSnapshot take(InputSnapshot &source) {
    auto result = source;
    for (auto &[name, state] : source.actions)
      state.pressed = state.released = state.canceled = false;
    return result;
  }
};

InputMap::InputMap() : _impl{std::make_unique<Impl>()} {}

InputMap::~InputMap() = default;

ContextId InputMap::addContext(InputContextProps props,
                               std::vector<Binding> bindings) {
  if (props.name.empty() || (props.stage != InputStage::BeforeUI &&
                             props.stage != InputStage::AfterUI))
    throw std::invalid_argument("Invalid input context");
  auto prepared = _impl->prepare(std::move(bindings));
  if (_impl->next == std::numeric_limits<ContextId>::max())
    throw std::overflow_error("Input context identities exhausted");
  const auto id = _impl->next++;
  _impl->contexts.push_back({id, std::move(props), std::move(prepared)});
  std::ranges::stable_sort(_impl->contexts, [](const auto &a, const auto &b) {
    return a.props.priority > b.props.priority;
  });
  _impl->publish();
  cancelAll();
  return id;
}

InputContextProps InputMap::contextProps(ContextId id) const {
  return _impl->find(id).props;
}

std::vector<Binding> InputMap::bindings(ContextId id) const {
  std::vector<Binding> result;
  for (const auto &bound : _impl->find(id).bindings)
    result.push_back(bound.props);
  return result;
}

void InputMap::setContextProps(ContextId id, InputContextProps props) {
  if (props.name.empty() || (props.stage != InputStage::BeforeUI &&
                             props.stage != InputStage::AfterUI))
    throw std::invalid_argument("Invalid input context");
  _impl->find(id).props = std::move(props);
  std::ranges::sort(_impl->contexts, [](const auto &a, const auto &b) {
    return a.props.priority != b.props.priority
               ? a.props.priority > b.props.priority
               : a.id < b.id;
  });
  cancelAll();
}

void InputMap::removeContext(ContextId id) {
  _impl->find(id);
  std::erase_if(_impl->contexts, [=](const auto &c) { return c.id == id; });
  _impl->publish(true);
  cancelAll();
}

void InputMap::setEnabled(ContextId id, bool enabled) {
  auto &c = _impl->find(id);
  if (c.props.enabled == enabled)
    return;
  _impl->suppress(c);
  c.props.enabled = enabled;
  _impl->publish(true);
  cancelAll();
}

void InputMap::rebind(ContextId id, std::vector<Binding> bindings) {
  auto &c = _impl->find(id);
  auto prepared = _impl->prepare(std::move(bindings), id);
  c.bindings = std::move(prepared);
  _impl->publish(true);
  cancelAll();
}

bool InputMap::route(const InputEvent &event, InputStage stage, bool blocked) {
  if (!std::isfinite(event.value) || std::abs(event.value) > 1 ||
      event.control.code < 0 ||
      (stage != InputStage::BeforeUI && stage != InputStage::AfterUI))
    throw std::invalid_argument("Input values must be normalized");
  if (stage == InputStage::BeforeUI) {
    if (event.value == 0)
      _impl->physical.erase(event.control);
    else
      _impl->physical[event.control] = event.value;
  }
  bool canceled{};
  for (auto &c : _impl->contexts) {
    if (c.props.stage != stage)
      continue;
    bool matched{};
    for (auto &b : c.bindings) {
      if (!Impl::matches(b.props, event.control))
        continue;
      const bool neutral = std::abs(event.value) <= b.props.deadZone;
      if (neutral) {
        const bool wasActive = b.values.erase(event.control) != 0;
        canceled |= blocked && wasActive;
        b.suppressed.erase(event.control);
      } else if (blocked || !c.props.enabled) {
        canceled |= b.values.erase(event.control) != 0;
        b.suppressed.insert(event.control);
      } else if (!b.suppressed.contains(event.control) && !event.repeat) {
        b.values[event.control] = std::copysign(
            (std::abs(event.value) - b.props.deadZone) / (1 - b.props.deadZone),
            event.value);
      }
      matched |= c.props.enabled;
    }
    blocked |= matched && c.props.consume;
  }
  _impl->publish(canceled);
  return blocked;
}

void InputMap::cancelAll() {
  for (auto &c : _impl->contexts)
    _impl->suppress(c);
  _impl->publish(true);
  for (auto *snapshot : {&_impl->frame, &_impl->tick})
    for (auto &[name, state] : snapshot->actions) {
      state.canceled |= state.pressed || state.released;
      state.pressed = state.released = false;
    }
}

void InputMap::cancelDevice(ControlKind kind, std::uint32_t device) {
  auto matches = [=](const auto &c) {
    return c.kind == kind && c.device == device;
  };
  std::erase_if(_impl->physical,
                [&](const auto &p) { return matches(p.first); });
  for (auto &c : _impl->contexts)
    for (auto &b : c.bindings) {
      std::erase_if(b.values, [&](const auto &p) { return matches(p.first); });
      std::erase_if(b.suppressed, matches);
    }
  _impl->publish(true);
  for (const auto &c : _impl->contexts)
    for (const auto &b : c.bindings)
      if (b.props.control == kind &&
          (!b.props.device || *b.props.device == device))
        for (auto *snapshot : {&_impl->frame, &_impl->tick}) {
          auto &state = snapshot->actions[b.props.action];
          state.canceled |= state.pressed || state.released;
          state.pressed = state.released = false;
        }
}

void InputMap::inheritHeld(const InputMap &other) {
  _impl->physical = other._impl->physical;
  cancelAll();
}

InputSnapshot InputMap::takeFrameSnapshot() { return Impl::take(_impl->frame); }

InputSnapshot InputMap::takeTickSnapshot() { return Impl::take(_impl->tick); }
} // namespace playground::input
