#pragma once

#include <optional>

#include <app/AppContext.hpp>
#include <app/AppTypes.hpp>
#include <input/InputMap.hpp>
#include <interfaces/IRuntimeObject.hpp>
#include <runtime/ActivationLifetime.hpp>
#include <runtime/Activity.hpp>
#include <runtime/CompletionQueue.hpp>
#include <runtime/SimulationClock.hpp>

class IApp : public IRuntimeObject {
  friend class AppHost;
  playground::runtime::ActivationLifetime _activation;
  playground::runtime::CompletionQueue _completions;
  std::optional<playground::runtime::SimulationClock> _simulation;
  bool _simulationPaused{};
  playground::input::InputMap _input;

public:
  virtual ~IApp() { _activation.deactivate(); }

  virtual AppInfo info() const = 0;

  virtual void configureLaunch(const AppLaunchProps &props) {
    if (props.benchmarkSeconds)
      throw std::invalid_argument("App does not accept benchmark options");
  }

  virtual playground::runtime::ActivityProps activityProps() const {
    return {};
  }

  virtual playground::runtime::ActivityDemand activityDemand() { return {}; }

  virtual playground::input::InputClaims inputClaims() { return {}; }

  playground::input::InputMap &input() noexcept { return _input; }

  playground::runtime::ActivationToken activationToken() const noexcept {
    return _activation.token();
  }

  playground::runtime::ActivationSink completions() const {
    return {_completions.sink(), activationToken()};
  }

  std::optional<playground::runtime::SimulationState> simulationState() const {
    return _simulation ? std::optional{_simulation->state()} : std::nullopt;
  }

  // Host overlays and focus loss interrupt exclusive interaction/workloads.
  virtual void onActivityInterrupted(AppContext &, AppInterruption) noexcept {}

  virtual void onActions(AppContext &,
                         const playground::input::InputSnapshot &) {}

  virtual std::optional<playground::runtime::SimulationTimingProps>
  simulationTiming() const {
    return {};
  }

  virtual void fixedUpdate(AppContext &, playground::runtime::SimulationStep,
                           const playground::input::InputSnapshot &) {}

  // Invalidate native resources after a published backend/domain change.
  // CPU/model state stays intact. No host commands, exceptions or onEnter
  // replay.
  virtual void
  onRendererChanged(AppContext &, playground::rendering::ResourceDomainId,
                    playground::rendering::ResourceDomainId) noexcept {}

  // Query only after onEnter has constructed content. No window mutations.
  // Null means this app has no preferred-content measurement implementation.
  virtual std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) {
    return {};
  }

  IApp(IApp &&) = delete;
  IApp &operator=(IApp &&) = delete;

  IApp(const IApp &) = delete;
  IApp &operator=(const IApp &) = delete;

protected:
  IApp() = default;
};
