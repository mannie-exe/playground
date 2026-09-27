#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <utility>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_timer.h>

#include <app/AppConfig.hpp>
#include <app/AppContext.hpp>
#include <app/AppHost.hpp>
#include <app/Assets.hpp>
#include <app/HostTransitions.hpp>
#include <demo2d/Assets.hpp>
#include <demo2d/Demo2DApp.hpp>
#include <demo3d/Demo3DApp.hpp>
#include <menu/MenuApp.hpp>
#include <minesweeper/Assets.hpp>
#include <minesweeper/MinesweeperApp.hpp>
#include <platform/sdl/SDLActionInput.hpp>
#include <rock_paper_scissors/RockPaperScissorsApp.hpp>
#include <snake/SnakeApp.hpp>
#include <support/Transaction.hpp>

AppHost::AppHost(WindowConfig initialWindow,
                 playground::rendering::RenderBackendProps backendProps)
    : _sdl{SDL_INIT_VIDEO | SDL_INIT_GAMEPAD}, _ttf{},
      _projectFiles{playground::platform::executableDirectory(), false},
      _userFiles{
          playground::platform::preferenceDirectory("Playground", "Playground"),
          true},
      _settings{_projectFiles, _userFiles},
      _session{initialWindow, backendProps} {
  auto catalog = std::make_shared<playground::assets::AssetCatalog>(
      playground::platform::executableDirectory() / "assets");
  playground::app::registerAssets(*catalog);
  playground::demo2d::registerAssets(*catalog);
  playground::minesweeper::registerAssets(*catalog);
  playground::demo3d::registerAssets(*catalog);
  catalog->freeze();
  _catalog = std::move(catalog);
  _resources =
      std::make_unique<playground::sdl::AssetResources>(_catalog, _assets);
  registerDefaultApps();
  _settings.reload();
  switchTo(AppId::Menu);
}

void AppHost::request(PendingAppCommand command) {
  if (_commandsSuppressed || command.type == AppCommandType::None)
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
  nextApp->_completions.setWakeCallback(_wake.callback());
  if (_activeApp)
    nextApp->input().inheritHeld(_activeApp->input());
  if (auto timing = nextApp->simulationTiming())
    nextApp->_simulation.emplace(*timing);
  const AppInfo nextInfo{nextApp->info()};
  auto nextPresentation = nextInfo.presentation;
  auto nextPolicy = nextInfo.view;
  _settings.resolve(appKey(appId), nextPresentation, nextPolicy);
  const auto previous = checkpoint();
  saveWindowSession();
  playground::app::activateApp(
      _activeApp, std::move(nextApp),
      [&] {
        configureRenderer(nextPresentation.renderer,
                          nextInfo.rendererRequirements);
      },
      [&] {
        _activeAppId = appId;
        _session.setPresentation(std::move(nextPresentation));
        _session.setViewPolicy(nextPolicy);
        _session.setWindowProps(nextInfo.window);
        _pendingCommand.reset();
        _session.prepareWindowForSizing();
        _session.applyWindowProps();
        _activeApp->_activation.activate();
        _activeApp->onEnter(ctx);
        applyViewSizing();
      },
      [&] { restore(previous); }, [&](IApp &app) noexcept { cleanupApp(app); });

  _notifiedRendererDomain = {};
  _paintRequest.request();
  _updateRequested = true;
  _updateClock.rebase();

  _assets.trim(playground::config::maxCachedFonts,
               playground::config::maxCachedImages,
               playground::config::maxCachedVectors);
  _assets.trimSurfaceBytes(playground::config::maxCachedSurfaceBytes);
  _resources->trimUnused();
}

void AppHost::cleanupApp(IApp &app) noexcept {
  app._activation.deactivate();
  // Exit hooks may not leak commands into the next app, even if they throw.
  try {
    playground::app::suppressCommands(_commandsSuppressed, [&] {
      AppContext ctx{*this, app};
      app.input().cancelAll();
      app.onExit(ctx);
    });
  } catch (const std::exception &error) {
    SDL_Log("App cleanup failed: %s", error.what());
  } catch (...) {
    SDL_Log("App cleanup failed with a non-standard exception");
  }
}

AppHost::RuntimeCheckpoint AppHost::checkpoint() {
  return {_activeAppId,
          _session.checkpoint(
              _activeApp ? _activeApp->info().rendererRequirements
                         : playground::rendering::RendererRequirements{}),
          _pendingCommand};
}

void AppHost::restore(const RuntimeCheckpoint &previous) {
  _updateClock.rebase();
  _activeAppId = previous.appId;
  _pendingCommand = previous.pending;
  _session.restore(previous.presentation);
}

void AppHost::recoverRenderer(std::string reason) {
  _session.recover(std::move(reason), _activeApp->info().rendererRequirements,
                   _updateClock, [&] { synchronizeRendererDomain(); });
}

void AppHost::synchronizeRendererDomain() {
  if (!_session.renderer() || !_activeApp)
    return;
  const auto current = _session.renderer()->resourceDomain();
  if (current == _notifiedRendererDomain)
    return;
  AppContext ctx{*this};
  _performance.setGPUTimingAvailable(false);
  _performance.setGPUTimingAvailable(_session.renderer()->supportsGPUTiming());
  _activeApp->onRendererChanged(ctx, _notifiedRendererDomain, current);
  _paintRequest.request();
  _notifiedRendererDomain = current;
}

void AppHost::applyViewSizing() {
  std::optional<playground::platform::SavedWindow> saved;
  if (const auto found = _settings.session().find(appKey(_activeAppId));
      found != _settings.session().end())
    saved = found->second;
  _session.applyViewSizing(saved, [&](auto maximum, auto scale) {
    return _activeApp->preferredContentSize(maximum, scale);
  });
}

int AppHost::run() {
  SDL_Event event;
  const std::uint64_t counterFrequency = SDL_GetPerformanceFrequency();
  std::uint64_t profilingRevision{};
  using Clock = playground::runtime::ActivityClock;
  auto retryAt = Clock::time_point::min();
  auto maintenanceAt = Clock::now();

  while (_running) {
    AppContext ctx{*this};
    const auto now = Clock::now();
    const auto activity = _activeApp->activityProps();
    const auto demand = _activeApp->activityDemand();
    const bool simulation = _activeApp->_simulation &&
                            !_activeApp->_simulationPaused && _inputFocused;
    const bool updateDue = activity.continuousUpdate || simulation ||
                           _updateRequested ||
                           _activeApp->_completions.pending() ||
                           demand.updateDue(now) || now >= maintenanceAt;
    const bool paintDue =
        now >= retryAt &&
        (activity.continuousPaint || _paintRequest.pending() || demand.paint);
    if (!updateDue && !paintDue && !_pendingCommand &&
        !SDL_PollEvent(nullptr)) {
      try {
        const auto samples = _session.renderer()->takeGPUTimings();
        _performance.recordGPUCollection(
            _session.renderer()->gpuTimingCollection());
        for (const auto &sample : samples)
          _performance.recordGPU(sample);
      } catch (const playground::rendering::RenderFailure &error) {
        recoverRenderer(error.what());
        continue;
      }
      auto deadline =
          std::min(maintenanceAt, now + std::chrono::milliseconds{100});
      if (demand.wakeAt)
        deadline = std::min(deadline, *demand.wakeAt);
      if (retryAt > now)
        deadline = std::min(deadline, retryAt);
      const auto waitStarted = Clock::now();
      const auto milliseconds =
          std::chrono::duration<double, std::milli>(deadline - waitStarted)
              .count();
      if (milliseconds <= 0)
        continue;
      SDL_WaitEventTimeout(
          nullptr, std::max(1, static_cast<int>(std::ceil(milliseconds))));
      _performance.recordIdleWait(
          std::chrono::duration<double, std::milli>(Clock::now() - waitStarted)
              .count());
      continue;
    }
    bool receivedEvent{};

    _performance.beginFrame();
    _performance.begin(FramePhase::Poll);

    while (SDL_PollEvent(&event)) {
      receivedEvent = true;
      const bool hostHandled{handleHostEvent(event)};

      if (!_running)
        break;

      if (_activeApp) {
        auto &input = _activeApp->input();
        playground::sdl::cancelActionInput(input, event);
        const auto action = playground::sdl::toActionInput(event);
        if (action)
          playground::input::routeInputEvent(
              input, *action, hostHandled || !_inputFocused, [&] {
                return _activeApp->handleEvent(ctx, event) !=
                       EventResult::Ignored;
              });
        else if (!hostHandled)
          _activeApp->handleEvent(ctx, event);
        _activeApp->onActions(ctx, input.takeFrameSnapshot());
      }

      processPendingCommand();

      if (!_running)
        break;
    }

    _performance.end(FramePhase::Poll);

    if (!_running)
      break;

    _performance.begin(FramePhase::Update);
    const bool shouldUpdate = updateDue || receivedEvent || _updateRequested;
    double frameSeconds{};
    _updateRequested = false;
    if (shouldUpdate) {
      if (_activeApp) {
        try {
          _activeApp->_completions.drain();
        } catch (const std::exception &error) {
          SDL_Log("App completion failed: %s", error.what());
        } catch (...) {
          SDL_Log("App completion failed with a non-standard exception");
        }
      }
      processPendingCommand();
      if (!_running)
        break;
      const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
      const double elapsed =
          _updateClock.advance(currentCounter, counterFrequency);
      const float deltaSeconds = static_cast<float>(elapsed);
      frameSeconds = elapsed;

      synchronizeRendererDomain();

      if (_activeApp && _activeApp->_simulation) {
        auto &clock = *_activeApp->_simulation;
        clock.setPaused(_activeApp->_simulationPaused || !_inputFocused);
        clock.beginFrame(elapsed);
        while (auto step = clock.nextStep())
          _activeApp->fixedUpdate(ctx, *step,
                                  _activeApp->input().takeTickSnapshot());
      }
      if (_activeApp)
        _activeApp->update(ctx, deltaSeconds);

      maintenanceAt = Clock::now() + std::chrono::seconds{1};
    }

    processPendingCommand();

    _performance.end(FramePhase::Update);

    if (!_running)
      break;

    synchronizeRendererDomain();
    const auto afterUpdate = _activeApp->activityDemand();
    const bool shouldPaint = Clock::now() >= retryAt &&
                             (_activeApp->activityProps().continuousPaint ||
                              _paintRequest.pending() || afterUpdate.paint);
    if (!shouldPaint) {
      try {
        const auto gpuTimings = _session.renderer()->takeGPUTimings();
        _performance.recordGPUCollection(
            _session.renderer()->gpuTimingCollection());
        for (const auto &sample : gpuTimings)
          _performance.recordGPU(sample);
      } catch (const playground::rendering::RenderFailure &error) {
        recoverRenderer(error.what());
      }
      _performance.endFrame();
      processPendingCommand();
      continue;
    }
    // Latch UI paint demand before root rendering can clear its dirty flag.
    _paintRequest.request();
    const auto paintRevision = _paintRequest.capture();
    _performance.begin(FramePhase::Render);
    auto timedPhase = FramePhase::Render;
    try {
      if (profilingRevision != _performance.statisticsRevision()) {
        _session.renderer()->setProfilingEnabled(false);
        profilingRevision = _performance.statisticsRevision();
      }
      _session.renderer()->setProfilingEnabled(_performance.isEnabled());
      _performance.setGPUTimingAvailable(
          _session.renderer()->supportsGPUTiming());
      const auto outcome = playground::app::renderFrame(
          *_session.renderer(),
          {.clearColor = _session.windowProps().clearColor,
           .settings = _session.presentation().render},
          [&](playground::rendering::RenderFrame &frame) {
            if (_activeApp)
              _activeApp->render(ctx, frame);
          },
          [&] {
            _performance.end(FramePhase::Render);
            _performance.begin(FramePhase::Present);
            timedPhase = FramePhase::Present;
          });
      _session.observeFrame(outcome, frameSeconds);
      if (outcome == playground::rendering::PresentationOutcome::Submitted)
        _paintRequest.submitted(paintRevision);
      else
        retryAt = Clock::now() + std::chrono::milliseconds{100};
      const auto gpuTimings = _session.renderer()->takeGPUTimings();
      _performance.recordGPUCollection(
          _session.renderer()->gpuTimingCollection());
      for (const auto &sample : gpuTimings)
        _performance.recordGPU(sample);
      if (auto work = _session.renderer()->takePaintWork())
        _performance.recordPaintWork(*work);
      _performance.end(FramePhase::Present);
    } catch (const playground::rendering::RenderFailure &error) {
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
  _registry.add(playground::demo2d::Demo2DApp::staticInfo(), [] {
    return std::make_unique<playground::demo2d::Demo2DApp>();
  });
  _registry.add(playground::demo3d::Demo3DApp::staticInfo(), [] {
    return std::make_unique<playground::demo3d::Demo3DApp>();
  });
  _registry.add(MinesweeperApp::staticInfo(),
                [] { return std::make_unique<MinesweeperApp>(); });
  _registry.add(RockPaperScissorsApp::staticInfo(),
                [] { return std::make_unique<RockPaperScissorsApp>(); });
  _registry.add(SnakeApp::staticInfo(),
                [] { return std::make_unique<SnakeApp>(); });
}

bool AppHost::handleHostEvent(const SDL_Event &event) {
  if (_wake.consume(event)) {
    _updateRequested = true;
    return true;
  }
  if (event.type >= SDL_EVENT_WINDOW_FIRST &&
      event.type <= SDL_EVENT_WINDOW_LAST) {
    _paintRequest.request();
  }
  _gamepads.handleEvent(event);
  if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
      event.type == SDL_EVENT_DID_ENTER_BACKGROUND) {
    _inputFocused = false;
    _updateClock.rebase();
  } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
    _inputFocused = true;
    _updateClock.rebase();
  }
  if (_activeApp && _activeApp->_simulation)
    _activeApp->_simulation->setPaused(_activeApp->_simulationPaused ||
                                       !_inputFocused);
  if (event.type == SDL_EVENT_KEY_DOWN &&
      _performance.handleHotkey(event.key)) {
    _paintRequest.request();
    return true;
  }

  _session.handleEvent(event);

  if (event.type == SDL_EVENT_QUIT)
    request(PendingAppCommand{.type = AppCommandType::Quit});

  return event.type == SDL_EVENT_QUIT;
}

void AppHost::setSimulationPaused(bool paused) {
  if (_commandsSuppressed || !_activeApp || !_activeApp->_simulation)
    return;
  if (_activeApp->_simulationPaused == paused)
    return;
  _activeApp->_simulationPaused = paused;
  _activeApp->_simulation->setPaused(paused || !_inputFocused);
  _activeApp->input().cancelAll();
  _updateClock.rebase();
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
    if (!_session.renderer() ||
        _session.recovery().status() ==
            playground::rendering::RecoveryStatus::Exhausted)
      throw;
    _lastCommandError = error.what();
    SDL_Log("Host command rejected: %s", error.what());
  }
}

void AppHost::executeCommand(PendingAppCommand command) {
  _paintRequest.request();
  _updateRequested = true;
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
            _session.setWindowProps(std::move(*command.window));
            _session.applyWindowProps();
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
            _session.setViewPolicy(*command.view);
            _session.prepareWindowForSizing();
            applyViewSizing();
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::FitContent:
    if (_session.presentation().window.mode ==
        playground::platform::WindowMode::Windowed) {
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            _session.fitContent([&](auto maximum, auto scale) {
              return _activeApp->preferredContentSize(maximum, scale);
            });
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::SetPresentation:
    if (command.presentation) {
      command.presentation->validate();
      const auto previous = checkpoint();
      playground::withRestoration(
          [&] {
            configureRenderer(command.presentation->renderer,
                              _activeApp->info().rendererRequirements);
            _session.setPresentation(*command.presentation);
            _session.applyPresentation();
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
          _session.applyPresentation();
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
            configureRenderer(candidate.renderer, info.rendererRequirements);
            _session.setPresentation(std::move(candidate));
            _session.setViewPolicy(policy);
            _session.applyPresentation();
            _settings.setUser(std::move(*command.settings), command.persist);
          },
          [&] { restore(previous); });
    }
    return;
  case AppCommandType::RecoverRenderer:
    _session.resetRecovery();
    recoverRenderer("Explicit renderer recovery requested");
    return;
  }
}

void AppHost::resolveSettings() {
  const auto info = _activeApp->info();
  auto presentation = info.presentation;
  auto policy = info.view;
  _settings.resolve(appKey(_activeAppId), presentation, policy);
  configureRenderer(presentation.renderer, info.rendererRequirements);
  _session.setPresentation(std::move(presentation));
  _session.setViewPolicy(policy);
}

void AppHost::configureRenderer(
    playground::rendering::RendererPreferences preferences,
    playground::rendering::RendererRequirements requirements) {
  if (_session.configureRenderer(preferences, requirements))
    _updateClock.rebase();
}

void AppHost::saveWindowSession() {
  if (!_activeApp)
    return;
  auto session = _settings.session();
  session.insert_or_assign(std::string{appKey(_activeAppId)},
                           _session.savedWindow());
  try {
    _settings.saveSession(std::move(session));
  } catch (const std::exception &error) {
    SDL_Log("Cannot save window session: %s", error.what());
  }
}
