#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <app/AppConfig.hpp>
#include <app/AppContext.hpp>
#include <app/AppHost.hpp>
#include <demo/DemoApp.hpp>
#include <menu/MenuApp.hpp>
#include <minesweeper/MinesweeperApp.hpp>
#include <platform/sdl/RenderBackendFactory.hpp>
#include <snake/SnakeApp.hpp>
#include <support/SDLError.hpp>
#include <support/Transaction.hpp>

AppHost::AppHost(WindowConfig initialWindow)
    : _sdl{SDL_INIT_VIDEO}, _ttf{},
      _projectFiles{playground::platform::executableDirectory(), false},
      _userFiles{
          playground::platform::preferenceDirectory("Playground", "Playground"),
          true},
      _settings{_projectFiles, _userFiles}, _window{initialWindow},
      _bootstrapSize{initialWindow.windowedSize} {
  registerDefaultApps();
  _settings.reload();
  switchTo(AppId::Menu);
}

void AppHost::request(PendingAppCommand command) {
  if (command.type == AppCommandType::None)
    return;

  if (command.type == AppCommandType::Quit || !_pendingCommand ||
      _pendingCommand->type != AppCommandType::Quit) {
    _pendingCommand = std::move(command);
  }
}

void AppHost::switchTo(AppId appId) {
  AppContext ctx{*this};

  // Check the next app's requirements before exiting the current app.
  std::unique_ptr<IApp> nextApp{_registry.create(appId)};
  const AppInfo nextInfo{nextApp->info()};
  auto nextPresentation = nextInfo.presentation;
  auto nextPolicy = nextInfo.view;
  _settings.resolve(appKey(appId), nextPresentation, nextPolicy);
  const auto previous = checkpoint();
  saveWindowSession();
  auto previousApp = std::move(_activeApp);
  playground::withRestoration(
      [&] {
        auto nextRenderer = configureRenderer(nextPresentation.renderer,
                                              nextInfo.rendererRequirements);
        _activeApp = std::move(nextApp);
        _activeAppId = appId;
        _presentation = std::move(nextPresentation);
        _viewPolicy = nextPolicy;
        _rendererState = std::move(nextRenderer);
        _windowProps = nextInfo.window;
        _pendingCommand.reset();
        prepareWindowForSizing();
        applyWindowProps(_windowProps);
        _activeApp->onEnter(ctx);
        applyViewSizing();
      },
      [&] {
        if (_activeApp)
          cleanupApp(*_activeApp);
        _activeApp = std::move(previousApp);
        restore(previous);
      });
  if (previousApp)
    cleanupApp(*previousApp);

  _notifiedRendererDomain = {};
  _updateClock.rebase();

  _assets.trim(playground::config::maxCachedFonts,
               playground::config::maxCachedImages,
               playground::config::maxCachedVectors);
  _assets.trimSurfaceBytes(playground::config::maxCachedSurfaceBytes);
}

void AppHost::cleanupApp(IApp &app) noexcept {
  // Exit hooks may not leak commands into the next app, even if they throw.
  auto pending = std::move(_pendingCommand);
  _pendingCommand.reset();
  try {
    AppContext ctx{*this};
    app.onExit(ctx);
  } catch (const std::exception &error) {
    SDL_Log("App cleanup failed: %s", error.what());
  } catch (...) {
    SDL_Log("App cleanup failed with a non-standard exception");
  }
  _pendingCommand = std::move(pending);
}

AppHost::RuntimeCheckpoint AppHost::checkpoint() {
  _window.refreshState();
  return {_activeAppId,
          _windowProps,
          _viewPolicy,
          _presentation,
          _rendererState,
          _activeApp ? _activeApp->info().rendererRequirements
                     : playground::rendering::RendererRequirements{},
          bool(_renderer),
          _window.state(),
          _pendingCommand};
}

void AppHost::restore(const RuntimeCheckpoint &previous) {
  _updateClock.rebase();
  _rendererRecovery.skipped();
  _activeAppId = previous.appId;
  _windowProps = previous.windowProps;
  _viewPolicy = previous.view;
  _presentation = previous.presentation;
  _rendererState = previous.renderer;
  _pendingCommand = previous.pending;
  _renderer.reset();
  if (SDL_WindowHasSurface(_window.get()) &&
      !SDL_DestroyWindowSurface(_window.get()))
    throwSDLError("Cannot release surface while restoring runtime");
  if (previous.hasRenderer) {
    _renderer =
        playground::sdl::createRenderBackend(*_window.get(), previous.renderer);
    _renderer->prepare(previous.requirements);
  }
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  normal.center = false;
  _window.setMinimumSize({1, 1});
  _window.applyPreferences(normal);
  _window.setWindowedSize(previous.window.windowedSize);
  _window.setWindowedPosition(previous.window.windowedPosition);
  applyWindowProps(_windowProps);
  applyPresentation(true);
  if (!previous.window.fullscreen) {
    _window.setMaximized(previous.window.maximized);
    _window.setMinimized(previous.window.minimized);
  }
}

void AppHost::recoverRenderer(std::string reason) {
  if (!_rendererRecovery.begin(std::move(reason)))
    throw playground::rendering::RenderFailure(
        "Renderer recovery attempt budget exhausted: " +
        _rendererRecovery.reason());
  try {
    if (_renderer)
      _renderer->invalidate();
    _renderer.reset();
    if (SDL_WindowHasSurface(_window.get()) &&
        !SDL_DestroyWindowSurface(_window.get()))
      throwSDLError("Cannot release surface during renderer recovery");
    auto replacement = playground::sdl::createRenderBackend(
        *_window.get(), _presentation.renderer,
        _activeApp->info().rendererRequirements);
    _renderer = std::move(replacement.backend);
    _rendererState = std::move(replacement.state);
    _rendererRecovery.recovered();
    _updateClock.rebase();
    synchronizeRendererDomain();
  } catch (...) {
    _rendererRecovery.failed();
    throw;
  }
}

void AppHost::synchronizeRendererDomain() {
  if (!_renderer || !_activeApp)
    return;
  const auto current = _renderer->resourceDomain();
  if (current == _notifiedRendererDomain)
    return;
  AppContext ctx{*this};
  _performance.setGPUTimingAvailable(false);
  _performance.setGPUTimingAvailable(_renderer->supportsGPUTiming());
  _activeApp->onRendererChanged(ctx, _notifiedRendererDomain, current);
  _notifiedRendererDomain = current;
}

void AppHost::prepareWindowForSizing() {
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  _window.setMinimumSize({1, 1});
  _window.applyPreferences(normal);
  _window.setWindowedSize(_viewPolicy.initialWindowSize(_bootstrapSize));
}

void AppHost::applyViewSizing() {
  using playground::platform::InitialWindowSizing;
  bool restored = false;
  if (_viewPolicy.initialSizing == InitialWindowSizing::FitContent)
    fitContent();
  else if (_viewPolicy.initialSizing == InitialWindowSizing::RestorePrevious) {
    if (const auto found = _settings.session().find(appKey(_activeAppId));
        found != _settings.session().end()) {
      auto preference = _presentation.window.display;
      if (preference.selection ==
              playground::platform::DisplaySelection::Current &&
          !found->second.displayName.empty())
        preference = {playground::platform::DisplaySelection::Named,
                      found->second.displayName};
      const auto area = _window.usableBounds(preference);
      const playground::math::Vec2i size{
          std::min(found->second.size.x, static_cast<int>(area.w())),
          std::min(found->second.size.y, static_cast<int>(area.h()))};
      _window.setWindowedSize(size);
      _window.setWindowedPosition(
          {std::clamp(found->second.position.x, static_cast<int>(area.x()),
                      static_cast<int>(area.right()) - size.x),
           std::clamp(found->second.position.y, static_cast<int>(area.y()),
                      static_cast<int>(area.bottom()) - size.y)});
      restored = true;
    }
  }
  applyPresentation(restored);
}

int AppHost::run() {
  SDL_Event event;
  const std::uint64_t counterFrequency = SDL_GetPerformanceFrequency();

  while (_running) {
    AppContext ctx{*this};

    _performance.beginFrame();
    _performance.begin(FramePhase::Poll);

    while (SDL_PollEvent(&event)) {
      const bool hostHandled{handleHostEvent(event)};

      if (!_running)
        break;

      if (!hostHandled && _activeApp)
        _activeApp->handleEvent(ctx, event);

      processPendingCommand();

      if (!_running)
        break;
    }

    _performance.end(FramePhase::Poll);

    if (!_running)
      break;

    _performance.begin(FramePhase::Update);
    const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
    const float deltaSeconds = static_cast<float>(
        _updateClock.advance(currentCounter, counterFrequency));

    synchronizeRendererDomain();

    if (_activeApp)
      _activeApp->update(ctx, deltaSeconds);

    processPendingCommand();

    _performance.end(FramePhase::Update);

    if (!_running)
      break;

    synchronizeRendererDomain();
    _performance.begin(FramePhase::Render);
    auto timedPhase = FramePhase::Render;
    std::unique_ptr<playground::rendering::RenderFrame> frame;
    try {
      _renderer->setProfilingEnabled(_performance.isEnabled());
      _performance.setGPUTimingAvailable(_renderer->supportsGPUTiming());
      frame = _renderer->beginFrame({.clearColor = _windowProps.clearColor,
                                     .settings = _presentation.render});
      if (frame && _activeApp)
        _activeApp->render(ctx, *frame);
      _performance.end(FramePhase::Render);
      _performance.begin(FramePhase::Present);
      timedPhase = FramePhase::Present;
      const auto outcome =
          frame ? frame->present()
                : playground::rendering::PresentationOutcome::Skipped;
      const auto completedWork = _renderer->completedWork();
      if (outcome == playground::rendering::PresentationOutcome::Submitted)
        _rendererRecovery.observeCompleted(completedWork, deltaSeconds);
      else
        _rendererRecovery.skipped();
      for (const auto &sample : _renderer->takeGPUTimings())
        _performance.recordGPU(sample);
      frame.reset();
      _performance.end(FramePhase::Present);
    } catch (const playground::rendering::RenderFailure &error) {
      frame.reset();
      _performance.end(timedPhase);
      recoverRenderer(error.what());
    }
    _performance.endFrame();
    processPendingCommand();
  }

  AppContext ctx{*this};
  saveWindowSession();
  if (_activeApp)
    cleanupApp(*_activeApp);

  return 0;
}

void AppHost::registerDefaultApps() {
  _registry.add(MenuApp::staticInfo(),
                [] { return std::make_unique<MenuApp>(); });
  _registry.add(DemoApp::staticInfo(),
                [] { return std::make_unique<DemoApp>(); });
  _registry.add(MinesweeperApp::staticInfo(),
                [] { return std::make_unique<MinesweeperApp>(); });
  _registry.add(SnakeApp::staticInfo(),
                [] { return std::make_unique<SnakeApp>(); });
}

bool AppHost::handleHostEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_KEY_DOWN && _performance.handleHotkey(event.key))
    return true;

  _window.handleEvent(event);

  if (event.type == SDL_EVENT_QUIT)
    request(PendingAppCommand{.type = AppCommandType::Quit});

  return event.type == SDL_EVENT_QUIT;
}

void AppHost::processPendingCommand() {
  if (!_pendingCommand)
    return;

  PendingAppCommand command = std::move(*_pendingCommand);
  _pendingCommand.reset();
  try {
    executeCommand(std::move(command));
    _lastCommandError.clear();
  } catch (const playground::RestorationFailure &) {
    throw;
  } catch (const std::exception &error) {
    if (!_renderer || _rendererRecovery.status() ==
                          playground::rendering::RecoveryStatus::Exhausted)
      throw;
    _lastCommandError = error.what();
    SDL_Log("Host command rejected: %s", error.what());
  }
}

void AppHost::executeCommand(PendingAppCommand command) {
  switch (command.type) {
  case AppCommandType::None:
    return;
  case AppCommandType::Quit:
    _running = false;
    return;
  case AppCommandType::SwitchTo:
    switchTo(command.target);
    return;
  case AppCommandType::ReturnToMenu:
    switchTo(AppId::Menu);
    return;
  case AppCommandType::SetWindowProps:
    if (command.window) {
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            applyWindowProps(*command.window);
            _windowProps = std::move(*command.window);
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::SetViewPolicy:
    if (command.view) {
      command.view->validate();
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            _viewPolicy = *command.view;
            prepareWindowForSizing();
            applyViewSizing();
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::FitContent:
    if (_presentation.window.mode ==
        playground::platform::WindowMode::Windowed) {
      const auto previous = checkpoint();
      playground::withRestoration([&] { fitContent(); },
                                  [&] { restore(previous); });
    }
    return;
  case AppCommandType::SetPresentation:
    if (command.presentation) {
      command.presentation->validate();
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            auto selected =
                configureRenderer(command.presentation->renderer,
                                  _activeApp->info().rendererRequirements);
            _presentation = *command.presentation;
            _rendererState = std::move(selected);
            applyPresentation();
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::ReloadSettings: {
    auto loaded = _settings.readSnapshot();
    auto oldSettings = _settings.snapshot();
    const auto previous = checkpoint();
    playground::withRestoration(
        [&] {
          _settings.publish(std::move(loaded));
          resolveSettings();
          applyPresentation();
        },
        [&] {
          _settings.publish(std::move(oldSettings));
          restore(previous);
        });
    return;
  }
  case AppCommandType::SetUserSettings:
    if (command.settings) {
      const auto info = _activeApp->info();
      auto candidate = info.presentation;
      auto policy = info.view;
      _settings.resolveWithUser(appKey(_activeAppId), *command.settings,
                                candidate, policy);
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            auto selected = configureRenderer(candidate.renderer,
                                              info.rendererRequirements);
            _presentation = std::move(candidate);
            _viewPolicy = policy;
            _rendererState = std::move(selected);
            applyPresentation();
            _settings.setUser(std::move(*command.settings), command.persist);
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::RecoverRenderer:
    _rendererRecovery = playground::rendering::RecoveryState{};
    recoverRenderer("Explicit renderer recovery requested");
    return;
  }
}

void AppHost::applyWindowProps(const AppWindowProps &config) {
  _window.setTitle(config.title);
  _window.setAlwaysOnTop(config.alwaysOnTop);
  _window.setFocusable(config.focusable);
  _window.setMouseGrabbed(config.mouseGrabbed);
  _window.setHidden(config.hidden);
}

void AppHost::resolveSettings() {
  const auto info = _activeApp->info();
  auto presentation = info.presentation;
  auto policy = info.view;
  _settings.resolve(appKey(_activeAppId), presentation, policy);
  auto selected =
      configureRenderer(presentation.renderer, info.rendererRequirements);
  _presentation = std::move(presentation);
  _viewPolicy = policy;
  _rendererState = std::move(selected);
}

playground::rendering::RendererState AppHost::resolveRenderer(
    playground::rendering::RendererPreferences preferences,
    playground::rendering::RendererRequirements requirements) const {
  return playground::sdl::resolveRenderer(preferences, requirements);
}

playground::rendering::RendererState AppHost::configureRenderer(
    playground::rendering::RendererPreferences preferences,
    playground::rendering::RendererRequirements requirements) {
  const auto selected = resolveRenderer(preferences, requirements);
  if (_renderer) {
    const auto current = _renderer->description();
    if (current.backend == selected.selected.backend &&
        current.driver == selected.selected.driver) {
      _renderer->prepare(requirements);
      return selected;
    }
  }
  _renderer.reset();
  // Surface and GPU presentation cannot simultaneously own this window.
  _updateClock.rebase();
  _rendererRecovery.skipped();
  if (SDL_WindowHasSurface(_window.get()) &&
      !SDL_DestroyWindowSurface(_window.get()))
    throwSDLError("Cannot release window surface for renderer switch");
  auto replacement = playground::sdl::createRenderBackend(
      *_window.get(), preferences, requirements);
  _renderer = std::move(replacement.backend);
  return std::move(replacement.state);
}

void AppHost::applyPresentation(bool preservePosition) {
  _presentation.validate();
  _viewPolicy.validate();
  auto preferences = _presentation.window;
  if (preservePosition) {
    preferences.center = false;
    preferences.display.selection =
        playground::platform::DisplaySelection::Current;
  }
  _window.applyPreferences(preferences);
  const auto factor = playground::platform::uiWindowScale(
      _presentation.viewport, _window.metrics());
  const auto minimum = _viewPolicy.minimumSize;
  const auto coordinate = [](double value) {
    if (!std::isfinite(value) || value > std::numeric_limits<int>::max())
      throw std::overflow_error("Window minimum size overflow");
    return std::max(1, static_cast<int>(std::ceil(value)));
  };
  const auto area = _window.usableBounds(preferences.display);
  _window.setMinimumSize(
      {coordinate(std::min(double(minimum.width) * factor.x, double(area.w()))),
       coordinate(
           std::min(double(minimum.height) * factor.y, double(area.h())))});
  _window.setResizable(_viewPolicy.resizable);
}

void AppHost::fitContent() {
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  _window.applyPreferences(normal);
  const auto area = _window.usableBounds(_presentation.window.display);
  const auto metrics = _window.metrics();
  const auto factor =
      playground::platform::uiWindowScale(_presentation.viewport, metrics);
  int top{}, left{}, bottom{}, right{};
  SDL_GetWindowBordersSize(_window.get(), &top, &left, &bottom, &right);
  const playground::math::Size2 available{
      std::max(1.0f, area.w() - left - right),
      std::max(1.0f, area.h() - top - bottom)};
  const playground::math::Size2 maximum{available.width / factor.x,
                                        available.height / factor.y};
  const auto pixelScale =
      playground::platform::resolveViewport(_presentation.viewport, metrics)
          .pixelsPerLogical;
  const auto preferred =
      _presentation.viewport.mode ==
              playground::platform::ViewportMode::FixedCanvas
          ? std::optional{_presentation.viewport.canvasSize}
          : _activeApp->preferredContentSize(maximum, pixelScale);
  if (!preferred)
    return;
  if (!playground::math::isFinite(*preferred) ||
      !playground::math::isNonNegative(*preferred))
    throw std::invalid_argument(
        "App returned an invalid preferred content size");
  _window.setWindowedSize(
      {static_cast<int>(std::clamp(
           std::ceil(double(std::max(preferred->width,
                                     _viewPolicy.minimumSize.width)) *
                     factor.x),
           1.0, double(available.width))),
       static_cast<int>(std::clamp(
           std::ceil(double(std::max(preferred->height,
                                     _viewPolicy.minimumSize.height)) *
                     factor.y),
           1.0, double(available.height)))});
  if (_presentation.window.center) {
    _window.applyPreferences(normal);
  }
}

void AppHost::saveWindowSession() {
  if (!_activeApp)
    return;
  _window.refreshState();
  const auto &state = _window.state();
  auto session = _settings.session();
  const char *name = SDL_GetDisplayName(state.display);
  session.insert_or_assign(
      std::string{appKey(_activeAppId)},
      playground::platform::SavedWindow{
          state.windowedSize, state.windowedPosition, name ? name : ""});
  try {
    _settings.saveSession(std::move(session));
  } catch (const std::exception &error) {
    SDL_Log("Cannot save window session: %s", error.what());
  }
}
