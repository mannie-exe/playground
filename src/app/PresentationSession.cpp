#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <app/HostTransitions.hpp>
#include <app/PresentationSession.hpp>
#include <platform/sdl/RenderBackendFactory.hpp>
#include <support/SDLError.hpp>

namespace playground::app {

PresentationSession::PresentationSession(WindowConfig window,
                                         rendering::RenderBackendProps backend)
    : _window{[&] {
        auto hidden = window;
        hidden.hidden = true;
        return hidden;
      }()},
      _windowServices{_window.get()}, _backendProps{backend},
      _bootstrapSize{window.windowedSize} {
  _window.setHidden(window.hidden);
}

void PresentationSession::prepareWindowForSizing() {
  // Measurement remains synchronous. Only numeric geometry crosses the native
  // transition boundary; no borrowed application callback is retained.
  _stagedSize = _viewPolicy.initialWindowSize(_bootstrapSize);
  _stagedPosition.reset();
  _stagedDisplay.reset();
  _restoreMinimized = false;
  _restoreMode.reset();
}

void PresentationSession::applyPresentation(bool preservePosition) {
  _presentation.validate();
  _viewPolicy.validate();
  auto preferences = _presentation.window;
  if (_restoreMode)
    preferences.mode = *_restoreMode;
  if (_stagedDisplay)
    preferences.display = *_stagedDisplay;
  if (preservePosition) {
    preferences.center = false;
  }
  const auto factor = playground::platform::uiWindowScale(
      _presentation.viewport, _window.metrics());
  const auto minimum = _viewPolicy.minimumSize;
  const auto coordinate = [](double value) {
    if (!std::isfinite(value) || value > std::numeric_limits<int>::max())
      throw std::overflow_error("Window minimum size overflow");
    return std::max(1, static_cast<int>(std::ceil(value)));
  };
  const auto area = _window.usableBounds(preferences.display);
  _window.requestPreferences(
      {.preferences = preferences,
       .windowedSize = _stagedSize,
       .windowedPosition = _stagedPosition,
       .minimumSize = {coordinate(std::min(double(minimum.width) * factor.x,
                                           double(area.w()))),
                       coordinate(std::min(double(minimum.height) * factor.y,
                                           double(area.h())))},
       .resizable = _viewPolicy.resizable,
       .minimized = _restoreMinimized});
  _stagedSize.reset();
  _stagedPosition.reset();
  _stagedDisplay.reset();
  _restoreMinimized = false;
  _restoreMode.reset();
}

void PresentationSession::applyWindowProps() {
  _window.setTitle(_windowProps.title);
  _window.setAlwaysOnTop(_windowProps.alwaysOnTop);
  _window.setFocusable(_windowProps.focusable);
  _window.setMouseGrabbed(_windowProps.mouseGrabbed);
  _window.setHidden(_windowProps.hidden);
}

void PresentationSession::fitContent(const Measure &measure) {
  if (auto size = measureContent(measure)) {
    _stagedSize = size;
    applyPresentation();
  }
}

std::optional<math::Vec2i>
PresentationSession::measureContent(const Measure &measure) const {
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
  const auto preferred = _presentation.viewport.mode ==
                                 playground::platform::ViewportMode::FixedCanvas
                             ? std::optional{_presentation.viewport.canvasSize}
                             : measure(maximum, pixelScale);
  if (!preferred)
    return std::nullopt;
  if (!playground::math::isFinite(*preferred) ||
      !playground::math::isNonNegative(*preferred))
    throw std::invalid_argument(
        "App returned an invalid preferred content size");
  return math::Vec2i{
      static_cast<int>(
          std::clamp(std::ceil(double(std::max(preferred->width,
                                               _viewPolicy.minimumSize.width)) *
                               factor.x),
                     1.0, double(available.width))),
      static_cast<int>(std::clamp(
          std::ceil(double(std::max(preferred->height,
                                    _viewPolicy.minimumSize.height)) *
                    factor.y),
          1.0, double(available.height)))};
}

void PresentationSession::applyViewSizing(
    const std::optional<platform::SavedWindow> &saved, const Measure &measure) {
  using playground::platform::InitialWindowSizing;
  bool restored = false;
  if (_viewPolicy.initialSizing == InitialWindowSizing::FitContent) {
    if (auto size = measureContent(measure))
      _stagedSize = size;
  } else if (_viewPolicy.initialSizing ==
             InitialWindowSizing::RestorePrevious) {
    if (saved) {
      auto preference = _presentation.window.display;
      if (preference.selection ==
              playground::platform::DisplaySelection::Current &&
          !saved->displayName.empty())
        preference = {playground::platform::DisplaySelection::Named,
                      saved->displayName};
      const auto area = _window.usableBounds(preference);
      const playground::math::Vec2i size{
          std::min(saved->size.x, static_cast<int>(area.w())),
          std::min(saved->size.y, static_cast<int>(area.h()))};
      _stagedSize = size;
      _stagedDisplay = preference;
      if (saved->position)
        _stagedPosition = math::Vec2i{
            std::clamp(saved->position->x, static_cast<int>(area.x()),
                       static_cast<int>(area.right()) - size.x),
            std::clamp(saved->position->y, static_cast<int>(area.y()),
                       static_cast<int>(area.bottom()) - size.y)};
      restored = saved->position.has_value() &&
                 _window.placementCapabilities().requestsSupported;
    }
  }
  applyPresentation(restored);
}

void PresentationSession::restore(const Checkpoint &previous) {
  _window.cancelTransition();
  _rendererRecovery.skipped();
  _windowProps = previous.windowProps;
  _viewPolicy = previous.view;
  _presentation = previous.presentation;
  _rendererState = previous.renderer;
  if (_renderer)
    _renderer->invalidate();
  _renderer.reset();
  if (SDL_WindowHasSurface(_window.get()) &&
      !SDL_DestroyWindowSurface(_window.get()))
    throwSDLError("Cannot release surface while restoring runtime");
  if (previous.hasRenderer) {
    _renderer = playground::sdl::createRenderBackend(
        *_window.get(), previous.renderer, _backendProps);
    _renderer->prepare(previous.requirements);
  }
  restoreWindow(previous);
}

void PresentationSession::restoreWindow(const Checkpoint &previous) {
  _window.cancelTransition();
  _windowProps = previous.windowProps;
  _viewPolicy = previous.view;
  _presentation = previous.presentation;
  _stagedSize = previous.window.windowedSize;
  _stagedPosition = previous.window.windowedPosition;
  _stagedDisplay.reset();
  if (const auto *name = SDL_GetDisplayName(previous.window.display))
    _stagedDisplay =
        platform::DisplayPreference{platform::DisplaySelection::Named, name};
  _restoreMinimized = previous.window.minimized;
  _restoreMode.reset();
  if (!previous.window.fullscreen && previous.window.maximized)
    _restoreMode = platform::WindowMode::Maximized;
  applyWindowProps();
  if (previous.pendingWindowRequest) {
    _window.requestPreferences(*previous.pendingWindowRequest);
    _stagedSize.reset();
    _stagedPosition.reset();
    _stagedDisplay.reset();
    _restoreMinimized = false;
    _restoreMode.reset();
  } else {
    applyPresentation(true);
  }
}

bool PresentationSession::configureRenderer(
    playground::rendering::RendererPreferences preferences,
    playground::rendering::RendererRequirements requirements) {
  const auto selected = sdl::resolveRenderer(preferences, requirements);
  if (_renderer) {
    const auto current = _renderer->description();
    if (current.backend == selected.selected.backend &&
        current.driver == selected.selected.driver) {
      _renderer->prepare(requirements);
      _rendererState = selected;
      return false;
    }
  }
  // Surface and GPU presentation cannot simultaneously own this window.
  _rendererRecovery.skipped();
  playground::rendering::RendererState state;
  playground::app::replaceBackend(
      _renderer,
      [&] {
        if (SDL_WindowHasSurface(_window.get()) &&
            !SDL_DestroyWindowSurface(_window.get()))
          throwSDLError("Cannot release window surface for renderer switch");
      },
      [&] {
        auto replacement = playground::sdl::createRenderBackend(
            *_window.get(), preferences, requirements, _backendProps);
        state = std::move(replacement.state);
        return std::move(replacement.backend);
      });
  _rendererState = std::move(state);
  return true;
}

void PresentationSession::recover(std::string reason,
                                  rendering::RendererRequirements requirements,
                                  runtime::UpdateClock &clock,
                                  const std::function<void()> &notify) {
  playground::app::recoverBackend(
      _renderer, _rendererRecovery, clock, std::move(reason),
      [&] {
        if (SDL_WindowHasSurface(_window.get()) &&
            !SDL_DestroyWindowSurface(_window.get()))
          throwSDLError("Cannot release surface during renderer recovery");
      },
      [&] {
        auto replacement = playground::sdl::createRenderBackend(
            *_window.get(), _presentation.renderer, requirements,
            _backendProps);
        _rendererState = std::move(replacement.state);
        return std::move(replacement.backend);
      },
      notify);
}

PresentationSession::Checkpoint
PresentationSession::checkpoint(rendering::RendererRequirements requirements) {
  _window.refreshState();
  return {
      _windowProps, _viewPolicy,     _presentation,   _rendererState,
      requirements, bool(_renderer), _window.state(), _window.pendingRequest()};
}

void PresentationSession::observeFrame(rendering::PresentationOutcome outcome,
                                       double seconds) {
  if (outcome == rendering::PresentationOutcome::Submitted)
    _rendererRecovery.observeCompleted(_renderer->completedWork(), seconds);
  else
    _rendererRecovery.skipped();
}

platform::SavedWindow PresentationSession::savedWindow() {
  _window.refreshState();
  const auto &state = _window.state();
  const char *name = SDL_GetDisplayName(state.display);
  return {state.windowedSize, state.windowedPosition, name ? name : ""};
}

} // namespace playground::app
