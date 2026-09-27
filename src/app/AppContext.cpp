#include <app/AppContext.hpp>
#include <app/AppHost.hpp>

void AppContext::requestUpdate() { _host.requestUpdate(); }

void AppContext::requestRepaint() { _host.requestRepaint(); }

std::function<void()> AppContext::wakeCallback() const {
  return _host.wakeCallback();
}

playground::sdl::WindowServices &AppContext::windowServices() {
  return _host.windowServices();
}

playground::runtime::ActivationSink AppContext::completions() {
  return _owner ? _owner->completions() : _host.completions();
}

playground::runtime::ActivationToken AppContext::activationToken() const {
  return _owner ? _owner->activationToken() : _host.activationToken();
}

std::optional<playground::runtime::SimulationState>
AppContext::simulationState() const {
  return _owner ? _owner->simulationState() : _host.simulationState();
}

void AppContext::setSimulationPaused(bool paused) {
  _host.setSimulationPaused(paused);
}

const WindowState &AppContext::windowState() const {
  return _host.windowState();
}

const AppWindowProps &AppContext::windowProps() const {
  return _host.windowProps();
}

AssetRegistry &AppContext::assets() { return _host.assets(); }

playground::sdl::AssetResources &AppContext::resources() {
  return _host.resources();
}

playground::runtime::Executor &AppContext::workers() { return _host.workers(); }

PerformanceMonitor &AppContext::performance() { return _host.performance(); }

void AppContext::requestSwitch(AppId appId) {
  _host.request(
      PendingAppCommand{.type = AppCommandType::SwitchTo, .target = appId});
}

void AppContext::requestMenu() {
  _host.request(PendingAppCommand{.type = AppCommandType::ReturnToMenu,
                                  .target = AppId::Menu});
}

void AppContext::requestQuit() {
  _host.request(PendingAppCommand{.type = AppCommandType::Quit});
}

void AppContext::requestWindowProps(AppWindowProps config) {
  _host.request(PendingAppCommand{.type = AppCommandType::SetWindowProps,
                                  .window = std::move(config)});
}

void AppContext::requestViewPolicy(playground::platform::AppViewPolicy policy) {
  policy.validate();
  _host.request(
      {.type = AppCommandType::SetViewPolicy, .view = std::move(policy)});
}

playground::platform::WindowMetrics AppContext::windowMetrics() const {
  return _host.windowMetrics();
}

const playground::platform::PresentationProps &
AppContext::presentation() const {
  return _host.presentation();
}

const playground::rendering::RendererState &AppContext::rendererState() const {
  return _host.rendererState();
}

const playground::rendering::RecoveryState &
AppContext::rendererRecovery() const {
  return _host.rendererRecovery();
}

const std::string &AppContext::lastCommandError() const {
  return _host.lastCommandError();
}

void AppContext::requestRendererRecovery() {
  _host.request({.type = AppCommandType::RecoverRenderer});
}

const playground::platform::AppViewPolicy &AppContext::viewPolicy() const {
  return _host.viewPolicy();
}

const playground::platform::SettingsDocument &AppContext::userSettings() const {
  return _host.userSettings();
}

void AppContext::requestFitContent() {
  _host.request({.type = AppCommandType::FitContent});
}

void AppContext::requestPresentation(
    playground::platform::PresentationProps props) {
  props.validate();
  _host.request({.type = AppCommandType::SetPresentation,
                 .presentation = std::move(props)});
}

void AppContext::requestReloadSettings() {
  _host.request({.type = AppCommandType::ReloadSettings});
}

void AppContext::requestUserSettings(
    playground::platform::SettingsDocument settings, bool persist) {
  _host.request({.type = AppCommandType::SetUserSettings,
                 .settings = std::move(settings),
                 .persist = persist});
}
