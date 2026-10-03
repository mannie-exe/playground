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
                 playground::rendering::RenderBackendProps backendProps,
                 playground::ui::SettingsViewFactory settingsView,
                 AppHostDirectories directories)
    : _sdl{SDL_INIT_VIDEO | SDL_INIT_GAMEPAD}, _ttf{},
      _projectFiles{directories.project.value_or(
                        playground::platform::executableDirectory()),
                    false},
      _userFiles{directories.user ? *directories.user
                                  : playground::platform::preferenceDirectory(
                                        "Playground", "Playground"),
                 true},
      _settings{_projectFiles, _userFiles},
      _renderRuntime{backendProps.resources},
      _session{initialWindow, backendProps},
      _settingsViewFactory{std::move(settingsView)} {
  auto catalog = std::make_shared<playground::assets::AssetCatalog>(
      directories.project.value_or(
          playground::platform::executableDirectory()) /
      "assets");
  playground::app::registerAssets(*catalog);
  playground::demo2d::registerAssets(*catalog);
  playground::minesweeper::registerAssets(*catalog);
  playground::demo3d::registerAssets(*catalog);
  catalog->freeze();
  _catalog = std::move(catalog);
  _resources =
      std::make_unique<playground::sdl::AssetResources>(_catalog, _assets);
  _hostCompletions.setWakeCallback(_wake.callback());
  _servicePump.setWakeCallback(_serviceWake.callback());
  registerDefaultApps();
  _settings.reload();
  _quality.configure(_settings.graphics());
  _renderRuntime.applyPatch({.budgets = _quality.state().requested.budgets,
                             .pacing = _quality.state().requested.pacing});
  switchTo(AppId::Menu);
}

AppHost::~AppHost() {
  if (_activeApp && _activeApp->activationToken().isActive())
    cleanupApp(*_activeApp);
  _servicePump.close();
}

void AppHost::request(PendingAppCommand command) {
  if (_commandsSuppressed || command.type == AppCommandType::None)
    return;

  if (command.type == AppCommandType::Quit || !_pendingCommand ||
      _pendingCommand->type != AppCommandType::Quit) {
    _pendingCommand = std::move(command);
  }
}

void AppHost::switchTo(AppId appId, AppLaunchProps launch) {
  if (_settingsView)
    showSettings(false);
  AppContext ctx{*this};

  // Check the next app's requirements before exiting the current app.
  std::unique_ptr<IApp> nextApp{_registry.create(appId)};
  nextApp->configureLaunch(launch);
  nextApp->_completions.setWakeCallback(_wake.callback());
  if (_activeApp)
    nextApp->input().inheritHeld(_activeApp->input());
  if (auto timing = nextApp->simulationTiming())
    nextApp->_simulation.emplace(*timing);
  const AppInfo nextInfo{nextApp->info()};
  auto nextPresentation = nextInfo.presentation;
  auto nextPolicy = nextInfo.view;
  _settings.resolve(appKey(appId), nextPresentation, nextPolicy);
  nextPresentation.render = _quality.state().requested.presentation;
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
        _activeApp->_services = _servicePump.scope();
        _activeApp->onEnter(ctx);
        applyViewSizing();
      },
      [&] { restore(previous); }, [&](IApp &app) noexcept { cleanupApp(app); });

  _performance.setWorkload(std::string{_activeApp->info().name});
  _quality.reset(_session.renderer()->resourceDomain(),
                 _session.renderer()->supportsGPUTiming());
  _notifiedRendererDomain = {};
  _paintRequest.request();
  _updateRequested = true;
  _updateClock.rebase();

  _assets.trim(playground::config::maxCachedFonts,
               playground::config::maxCachedImages,
               playground::config::maxCachedVectors);
  _assets.trimSurfaceBytes(playground::config::maxCachedSurfaceBytes);
  _resources->trimUnused();
  _session.renderer()->trimUnused();
}

void AppHost::cleanupApp(IApp &app) noexcept {
  app._activation.deactivate();
  // Exit hooks may not leak commands into the next app, even if they throw.
  try {
    playground::app::suppressCommands(_commandsSuppressed, [&] {
      AppContext ctx{*this, app};
      app._services.close();
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
  _renderRuntime.attachDomain(current);
  _quality.reset(current, _session.renderer()->supportsGPUTiming());
  _lastCompletedWork = 0;
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

void AppHost::advanceWindowTransition(
    playground::platform::WindowTransition::TimePoint now) {
  if (_session.advanceWindowTransition(now)) {
    requestUpdate();
    requestRepaint();
  }
  const auto &status = _session.windowRequestStatus();
  using playground::platform::WindowTransitionOutcome;
  if (status.generation != _reportedWindowRequest &&
      status.outcome != WindowTransitionOutcome::Pending &&
      status.outcome != WindowTransitionOutcome::Idle) {
    _reportedWindowRequest = status.generation;
    if (!status.diagnostic.empty())
      SDL_Log("Window request %llu: %s",
              static_cast<unsigned long long>(status.generation),
              status.diagnostic.c_str());
  }
}

int AppHost::run() {
  SDL_Event event;
  const std::uint64_t counterFrequency = SDL_GetPerformanceFrequency();
  using Clock = playground::runtime::ActivityClock;
  auto retryAt = Clock::time_point::min();
  auto maintenanceAt = Clock::now();
  auto updateAt = Clock::now();

  while (_running) {
    AppContext ctx{*this};
    const auto now = Clock::now();
    try {
      processSettings();
      if (_pendingRuntimePatch) {
        bool trim{};
        if (const auto &next = _pendingRuntimePatch->budgets) {
          const auto old = _renderRuntime.resources()->snapshot().budgets;
          trim = next->cpuBytes < old.cpuBytes ||
                 next->gpuBytes < old.gpuBytes ||
                 next->targetBytes < old.targetBytes;
        }
        _renderRuntime.applyPatch(*_pendingRuntimePatch);
        _pendingRuntimePatch.reset();
        _paintRequest.request();
        if (trim)
          _session.renderer()->trimUnused();
      }
      collectRendererTelemetry();
    } catch (const playground::rendering::RenderFailure &error) {
      recoverRenderer(error.what());
      continue;
    }
    advanceWindowTransition(now);
    const auto activity = _settingsView
                              ? playground::runtime::ActivityProps{false, false}
                              : _activeApp->activityProps();
    const auto demand = _settingsView ? _settingsUI.activityDemand()
                                      : _activeApp->activityDemand();
    const bool simulation = !_settingsView && _activeApp->_simulation &&
                            !_activeApp->_simulationPaused && _inputFocused;
    const auto serviceDemand = _servicePump.demand();
    const auto admission =
        _renderRuntime.admission(now, _paintRequest.capture());
    const bool paintDue =
        admission.status ==
            playground::rendering::FrameAdmissionStatus::Ready &&
        now >= retryAt &&
        (activity.continuousPaint || _paintRequest.pending() || demand.paint);
    const bool updateDue =
        (activity.continuousUpdate && paintDue) ||
        ((activity.continuousUpdate || simulation) && now >= updateAt) ||
        _updateRequested || _hostCompletions.pending() ||
        _activeApp->_completions.pending() || demand.updateDue(now) ||
        now >= maintenanceAt;
    if (!updateDue && !paintDue && !serviceDemand.due(now) &&
        !_pendingCommand && !SDL_PollEvent(nullptr)) {
      auto deadline =
          std::min(maintenanceAt, now + std::chrono::milliseconds{100});
      if (_settingsView)
        deadline = std::min(deadline, _settingsMetersAt);
      if ((activity.continuousUpdate || simulation) && updateAt > now)
        deadline = std::min(deadline, updateAt);
      if (admission.wakeAt)
        deadline = std::min(deadline, *admission.wakeAt);
      if (_session.renderer()->pendingWork())
        deadline = std::min(deadline, now + std::chrono::milliseconds{2});
      if (demand.wakeAt)
        deadline = std::min(deadline, *demand.wakeAt);
      if (serviceDemand.wakeAt)
        deadline = std::min(deadline, *serviceDemand.wakeAt);
      if (const auto wakeAt = _session.windowTransitionWakeAt())
        deadline = std::min(deadline, *wakeAt);
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
      const auto idle =
          std::chrono::duration<double, std::milli>(Clock::now() - waitStarted)
              .count();
      _renderRuntime.recordIdle(idle);
      _performance.recordIdleWait(idle);
      continue;
    }
    bool receivedEvent{};

    _performance.beginFrame(); // optional report/UI capture
    _renderRuntime.beginIteration();
    _renderRuntime.begin(FramePhase::Poll);

    while (SDL_PollEvent(&event)) {
      if (_serviceWake.consume(event))
        continue;
      receivedEvent = true;
      const bool hostHandled{handleHostEvent(event)};

      if (!_running)
        break;

      if (_settingsView) {
        auto &input = _activeApp->input();
        playground::sdl::cancelActionInput(input, event);
        if (const auto action = playground::sdl::toActionInput(event))
          playground::input::routeInputEvent(input, *action, true,
                                             [] { return false; });
        if (!hostHandled) {
          const auto result = _settingsUI.handleEvent(event);
          if (result == EventResult::Ignored &&
              event.type == SDL_EVENT_KEY_DOWN &&
              _performance.handleHotkey(event.key))
            _paintRequest.request();
          if (result == EventResult::Ignored &&
              event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
              event.key.scancode == SDL_SCANCODE_ESCAPE)
            requestSettings(false);
        }
        continue;
      }
      if (_activeApp) {
        auto &input = _activeApp->input();
        input.setUIClaims(_activeApp->inputClaims());
        playground::sdl::cancelActionInput(input, event);
        const auto action = playground::sdl::toActionInput(event);
        if (action)
          playground::input::routeInputEvent(
              input, *action, hostHandled || !_inputFocused, [&] {
                bool handled =
                    _activeApp->handleEvent(ctx, event) != EventResult::Ignored;
                if (!handled && event.type == SDL_EVENT_KEY_DOWN &&
                    !event.key.repeat &&
                    event.key.scancode == SDL_SCANCODE_ESCAPE) {
                  requestSettings(true);
                  handled = true;
                }
                if (!handled && event.type == SDL_EVENT_KEY_DOWN &&
                    _performance.handleHotkey(event.key)) {
                  _paintRequest.request();
                  handled = true;
                }
                input.setUIClaims(_activeApp->inputClaims());
                return handled;
              });
        else if (!hostHandled)
          _activeApp->handleEvent(ctx, event);
        input.setUIClaims(_activeApp->inputClaims());
        _activeApp->onActions(ctx, input.takeFrameSnapshot());
      }

      processPendingCommand();

      if (!_running)
        break;
    }

    _renderRuntime.end(FramePhase::Poll);

    if (!_running)
      break;

    if (receivedEvent)
      _renderRuntime.retryPressure();
    advanceWindowTransition(Clock::now());

    _renderRuntime.begin(FramePhase::Update);
    _servicePump.advance(Clock::now());
    for (const auto &failure : _servicePump.takeFailures()) {
      try {
        std::rethrow_exception(failure.error);
      } catch (const std::exception &error) {
        SDL_Log("Service %s (%llu) failed: %s", failure.name.c_str(),
                static_cast<unsigned long long>(failure.id), error.what());
      } catch (...) {
        SDL_Log("Service %s (%llu) failed with a non-standard exception",
                failure.name.c_str(),
                static_cast<unsigned long long>(failure.id));
      }
    }
    const bool shouldUpdate = updateDue || receivedEvent || _updateRequested;
    double frameSeconds{};
    _updateRequested = false;
    if (shouldUpdate) {
      try {
        _hostCompletions.drain();
      } catch (const std::exception &error) {
        SDL_Log("Host completion failed: %s", error.what());
      }
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

      synchronizeInputClaims(ctx);

      if (!_settingsView && _activeApp && _activeApp->_simulation) {
        auto &clock = *_activeApp->_simulation;
        clock.setPaused(_activeApp->_simulationPaused || !_inputFocused);
        clock.beginFrame(elapsed);
        while (auto step = clock.nextStep()) {
          synchronizeInputClaims(ctx);
          _activeApp->fixedUpdate(ctx, *step,
                                  _activeApp->input().takeTickSnapshot());
        }
      }
      if (_settingsView) {
        _settingsUI.update(deltaSeconds);
        _settingsUI.synchronize(ctx);
      } else if (_activeApp)
        _activeApp->update(ctx, deltaSeconds);
      synchronizeInputClaims(ctx);

      // Update deadlines remain independent of the rendering cap. Fixed-step
      // simulation determines its own cadence; continuous variable updates use
      // the existing default simulation interval when no clock is installed.
      const double updateSeconds =
          _activeApp->_simulation
              ? _activeApp->_simulation->props().stepSeconds
              : playground::runtime::SimulationTimingProps{}.stepSeconds;
      updateAt =
          Clock::now() + std::chrono::duration_cast<Clock::duration>(
                             std::chrono::duration<double>{updateSeconds});
      maintenanceAt = Clock::now() + std::chrono::seconds{1};
    }

    processPendingCommand();

    _renderRuntime.end(FramePhase::Update);

    if (!_running)
      break;

    synchronizeRendererDomain();
    const auto afterUpdate = _settingsView ? _settingsUI.activityDemand()
                                           : _activeApp->activityDemand();
    const bool shouldPaint =
        _renderRuntime.admission(Clock::now(), _paintRequest.capture())
                .status == playground::rendering::FrameAdmissionStatus::Ready &&
        Clock::now() >= retryAt &&
        ((!_settingsView && _activeApp->activityProps().continuousPaint) ||
         _paintRequest.pending() || afterUpdate.paint);
    if (!shouldPaint) {
      const auto &sample = _renderRuntime.endIteration();
      if (!_settingsView)
        _quality.observeCPU(sample);
      _performance.recordFrame(sample);
      processPendingCommand();
      continue;
    }
    // Latch UI paint demand before root rendering can clear its dirty flag.
    if (!_paintRequest.pending())
      _paintRequest.request();
    const auto paintRevision = _paintRequest.capture();
    _renderRuntime.begin(FramePhase::Render);
    auto timedPhase = FramePhase::Render;
    bool terminalOutcome{};
    const auto blockRendering = [&](const std::string &reason,
                                    bool memoryPressure = true) {
      _renderRuntime.end(timedPhase);
      _renderRuntime.abandoned();
      const bool firstReclaim = _pressureRetriedRevision != paintRevision;
      if (firstReclaim)
        _renderRuntime.block(reason, paintRevision);
      try {
        _session.renderer()->trimUnused();
      } catch (const playground::rendering::RenderFailure &failure) {
        recoverRenderer(failure.what());
        return;
      }
      if (!firstReclaim)
        _renderRuntime.block(reason, paintRevision);
      if (memoryPressure && !_settingsView &&
          _activeApp->info().rendererRequirements.scene3D &&
          _quality.pressure()) {
        _renderRuntime.retryPressure();
        requestRepaint();
      }
      _pressureRetriedRevision = paintRevision;
      if (_reportedResourcePressure != reason) {
        _reportedResourcePressure = reason;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Rendering blocked: %s",
                    reason.c_str());
      }
    };
    try {
      auto admitted = _renderRuntime.beginFrame(
          Clock::now(), _session.renderer()->resourceDomain(), paintRevision);
      if (!admitted)
        throw std::logic_error("Frame admission changed on owner thread");
      const auto outcome = playground::app::renderFrame(
          *_session.renderer(),
          {.clearColor = _session.windowProps().clearColor,
           .settings =
               _settingsView
                   ? playground::rendering::
                         RenderSettings{.glyphAtlases =
                                            _quality.state()
                                                .requested.presentation
                                                .glyphAtlases,
                                        .vsync =
                                            _quality.state()
                                                .requested.presentation.vsync}
                   : _session.presentation().render,
           .admission = std::move(admitted)},
          [&](playground::rendering::RenderFrame &frame) {
            if (_settingsView) {
              _settingsUI.synchronize(ctx);
              _settingsUI.render(frame);
            } else if (_activeApp)
              _activeApp->render(ctx, frame);
          },
          [&] {
            _renderRuntime.end(FramePhase::Render);
            _renderRuntime.begin(FramePhase::Present);
            timedPhase = FramePhase::Present;
          });
      if (outcome == playground::rendering::PresentationOutcome::Submitted) {
        _paintRequest.submitted(paintRevision);
        _renderRuntime.submitted();
        _reportedResourcePressure.clear();
        _pressureRetriedRevision.reset();
      } else {
        _renderRuntime.skipped();
        retryAt = Clock::now() + std::chrono::milliseconds{16};
      }
      terminalOutcome = true;
      _session.observeFrame(outcome, frameSeconds);
      collectRendererTelemetry();
      _renderRuntime.end(FramePhase::Present);
    } catch (const playground::rendering::ResourcePressure &error) {
      blockRendering(error.what(),
                     error.unit ==
                         playground::rendering::ResourcePressure::Unit::Bytes);
    } catch (const playground::rendering::ResourceAllocationFailure &error) {
      blockRendering(error.what());
    } catch (const playground::rendering::RenderFailure &error) {
      if (!terminalOutcome)
        _renderRuntime.abandoned();
      _renderRuntime.end(timedPhase);
      recoverRenderer(error.what());
    }
    if (auto work = _session.renderer()->takePaintWork()) {
      _performance.recordPaintWork(*work);
      _renderRuntime.recordSceneWork(work->scene);
    }
    const auto &sample = _renderRuntime.endIteration();
    if (!_settingsView)
      _quality.observeCPU(sample);
    _performance.recordFrame(sample);
    processPendingCommand();
  }

  AppContext ctx{*this};
  if (_settingsView)
    showSettings(false);
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
  for (auto kind : {playground::demo3d::DemoKind::Bistro,
                    playground::demo3d::DemoKind::Chess,
                    playground::demo3d::DemoKind::Benchmark})
    _registry.add(playground::demo3d::Demo3DApp::staticInfo(kind), [kind] {
      return std::make_unique<playground::demo3d::Demo3DApp>(kind);
    });
  _registry.add(MinesweeperApp::staticInfo(),
                [] { return std::make_unique<MinesweeperApp>(); });
  _registry.add(RockPaperScissorsApp::staticInfo(),
                [] { return std::make_unique<RockPaperScissorsApp>(); });
  _registry.add(SnakeApp::staticInfo(),
                [] { return std::make_unique<SnakeApp>(); });
}

void AppHost::synchronizeInputClaims(AppContext &ctx) {
  if (!_settingsView && _activeApp &&
      _activeApp->input().setUIClaims(_activeApp->inputClaims()))
    _activeApp->onActions(ctx, _activeApp->input().takeFrameSnapshot());
}

bool AppHost::handleHostEvent(const SDL_Event &event) {
  if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
    if (event.key.scancode == SDL_SCANCODE_M &&
        (event.key.mod & SDL_KMOD_SHIFT) &&
        (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
      request(PendingAppCommand{.type = AppCommandType::ReturnToMenu});
      return true;
    }
  }
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
    _session.windowServices().releaseRelativeMouse();
    if (_activeApp) {
      AppContext ctx{*this};
      _activeApp->onActivityInterrupted(ctx, AppInterruption::Focus);
    }
    _inputFocused = false;
    _updateClock.rebase();
  } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
    _inputFocused = true;
    _updateClock.rebase();
  }
  if (_activeApp && _activeApp->_simulation)
    _activeApp->_simulation->setPaused(_activeApp->_simulationPaused ||
                                       !_inputFocused || _settingsView);

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
  _activeApp->_simulation->setPaused(paused || !_inputFocused || _settingsView);
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
    switchTo(command.target, command.launch);
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
          _quality.configure(_settings.graphics());
          _renderRuntime.applyPatch(
              {.budgets = _quality.state().requested.budgets,
               .pacing = _quality.state().requested.pacing});
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
            const auto graphics = _settings.graphics(*command.settings);
            _settings.setUser(std::move(*command.settings), command.persist);
            _quality.configure(graphics);
            _renderRuntime.applyPatch(
                {.budgets = graphics.budgets, .pacing = graphics.pacing});
          },
          [&] { restore(previous); });
      _activeApp->input().cancelAll();
      AppContext context{*this, *_activeApp};
      _activeApp->onActivityInterrupted(context, AppInterruption::Settings);
      if (_settingsView) {
        _settingsView->setControls(controlsState());
        _settingsView->setResult(_quality.state().requested,
                                 command.persist
                                     ? "Saved shared settings."
                                     : "Applied shared settings for this run.");
      }
      requestRepaint();
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
  presentation.render = _settings.graphics().presentation;
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

void AppHost::collectRendererTelemetry() {
  auto &backend = *_session.renderer();
  const auto samples = backend.takeGPUTimings();
  const auto collection = backend.gpuTimingCollection();
  _renderRuntime.recordGPUCollection(collection);
  _performance.recordGPUCollection(collection);
  for (const auto &sample : samples) {
    _renderRuntime.recordGPU(sample);
    _performance.recordGPU(sample);
    if (!_settingsView && _quality.observe(sample, &_renderRuntime.pacing()))
      requestRepaint();
  }
  const auto completed = backend.completedWork();
  if (completed != _lastCompletedWork) {
    if (!_reportedResourcePressure.empty())
      backend.trimUnused();
    _lastCompletedWork = completed;
  }
}

void AppHost::applyGraphics(playground::rendering::GraphicsSettings value,
                            bool persist) {
  value.validate();
  auto document = _settings.user();
  document.graphics = value;
  const auto previous = checkpoint();
  playground::withRestoration(
      [&] {
        configureRenderer(value.renderer,
                          _activeApp->info().rendererRequirements);
        auto presentation = _session.presentation();
        presentation.render = value.presentation;
        presentation.renderer = value.renderer;
        _session.setPresentation(presentation);
        _session.applyPresentation();
        auto policy = _session.viewPolicy();
        policy.colorScheme = value.colorScheme;
        policy.userContrast = value.contrast;
        _session.setViewPolicy(policy);
        _settings.setUser(std::move(document), persist);
      },
      [&] { restore(previous); });
  _quality.configure(value);
  _renderRuntime.applyPatch({.budgets = value.budgets, .pacing = value.pacing});
  _session.renderer()->trimUnused();
  requestRepaint();
  if (_settingsView)
    _settingsView->setResult(
        value, persist
                   ? "Saved. Settings apply to every app and future launches."
                   : "Applied to every app for this run. Use Save to persist.");
}

void AppHost::showSettings(bool visible) {
  using namespace playground;
  if (visible == bool(_settingsView) || (visible && !_settingsViewFactory))
    return;
  AppContext ctx{*this};
  _performance.setWorkload(visible ? "Settings"
                                   : std::string{_activeApp->info().name});
  _session.windowServices().cancelInput();
  if (visible)
    _activeApp->onActivityInterrupted(ctx, AppInterruption::Settings);
  _activeApp->input().cancelAll();
  if (_activeApp->_simulation)
    _activeApp->_simulation->setPaused(
        visible || _activeApp->_simulationPaused || !_inputFocused);
  _updateClock.rebase();
  if (!visible) {
    _settingsUI.clear();
    _settingsView = nullptr;
    auto saved = std::move(*_settingsWindow);
    _settingsWindow.reset();
    saved.presentation.render = _quality.state().requested.presentation;
    saved.presentation.renderer = _quality.state().requested.renderer;
    saved.view.colorScheme = _session.viewPolicy().colorScheme;
    saved.view.userContrast = _session.viewPolicy().userContrast;
    _session.restoreWindow(saved);
  } else {
    auto view = _settingsViewFactory(
        _assets, _resources->font(app::fontAsset, {.style = {.size = 18}}),
        _quality.state().requested,
        {.apply =
             [this](auto value, bool persist) {
               requestGraphics(std::move(value), persist);
             },
         .close = [this] { requestSettings(false); },
         .returnToMenu =
             [this] {
               request(PendingAppCommand{.type = AppCommandType::ReturnToMenu});
             },
         .controls = controlsState(),
         .applyShared =
             [this](auto graphics, auto controls, bool persist) {
               graphics.validate();
               controls.validate();
               auto document = _settings.user();
               document.graphics = graphics;
               document.controls = controls;
               request(
                   PendingAppCommand{.type = AppCommandType::SetUserSettings,
                                     .settings = std::move(document),
                                     .persist = persist});
             }});
    if (!view)
      throw std::invalid_argument("Settings view factory returned no view");
    auto previous =
        _session.checkpoint(_activeApp->info().rendererRequirements);
    _settingsView = view.get();
    _settingsUI.root().setContent(std::move(view));
    try {
      auto props = _session.presentation();
      props.viewport = {};
      _session.setPresentation(props);
      auto window = _session.windowProps();
      window.mouseGrabbed = false;
      _session.setWindowProps(window);
      _session.applyWindowProps();
      _settingsUI.synchronize(ctx);
      _settingsWindow = std::move(previous);
    } catch (...) {
      _settingsUI.clear();
      _settingsView = nullptr;
      _session.restoreWindow(previous);
      throw;
    }
  }
  requestRepaint();
  requestUpdate();
}

void AppHost::processSettings() {
  try {
    if (auto pending = std::exchange(_pendingGraphics, {}))
      applyGraphics(std::move(pending->first), pending->second);
    if (auto visible = std::exchange(_pendingSettingsVisible, {}))
      showSettings(*visible);
  } catch (const playground::RestorationFailure &) {
    throw;
  } catch (const std::exception &error) {
    _lastCommandError = error.what();
    if (_settingsView)
      _settingsView->setResult(_quality.state().requested, error.what());
    SDL_Log("Settings request rejected: %s", error.what());
  }
  const auto now = playground::runtime::ActivityClock::now();
  if (_settingsView && now >= _settingsMetersAt) {
    _settingsView->setRuntime(_quality.state(), _renderRuntime.snapshot());
    _settingsMetersAt = now + std::chrono::milliseconds{500};
  }
}
