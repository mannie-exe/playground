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
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  _window.setMinimumSize({1, 1});
  _window.applyPreferences(normal);
  _window.setWindowedSize(_viewPolicy.initialWindowSize(_bootstrapSize));
}

void PresentationSession::applyPresentation(bool preservePosition) {
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

void PresentationSession::applyWindowProps() {
  _window.setTitle(_windowProps.title);
  _window.setAlwaysOnTop(_windowProps.alwaysOnTop);
  _window.setFocusable(_windowProps.focusable);
  _window.setMouseGrabbed(_windowProps.mouseGrabbed);
  _window.setHidden(_windowProps.hidden);
}

void PresentationSession::fitContent(const Measure &measure) {
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
  const auto preferred = _presentation.viewport.mode ==
                                 playground::platform::ViewportMode::FixedCanvas
                             ? std::optional{_presentation.viewport.canvasSize}
                             : measure(maximum, pixelScale);
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

void PresentationSession::applyViewSizing(
    const std::optional<platform::SavedWindow> &saved, const Measure &measure) {
  using playground::platform::InitialWindowSizing;
  bool restored = false;
  if (_viewPolicy.initialSizing == InitialWindowSizing::FitContent)
    fitContent(measure);
  else if (_viewPolicy.initialSizing == InitialWindowSizing::RestorePrevious) {
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
      _window.setWindowedSize(size);
      _window.setWindowedPosition(
          {std::clamp(saved->position.x, static_cast<int>(area.x()),
                      static_cast<int>(area.right()) - size.x),
           std::clamp(saved->position.y, static_cast<int>(area.y()),
                      static_cast<int>(area.bottom()) - size.y)});
      restored = true;
    }
  }
  applyPresentation(restored);
}

void PresentationSession::restore(const Checkpoint &previous) {
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
  auto normal = _presentation.window;
  normal.mode = playground::platform::WindowMode::Windowed;
  normal.center = false;
  _window.setMinimumSize({1, 1});
  _window.applyPreferences(normal);
  _window.setWindowedSize(previous.window.windowedSize);
  _window.setWindowedPosition(previous.window.windowedPosition);
  applyWindowProps();
  applyPresentation(true);
  if (!previous.window.fullscreen) {
    _window.setMaximized(previous.window.maximized);
    _window.setMinimized(previous.window.minimized);
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
  return {_windowProps, _viewPolicy,     _presentation,  _rendererState,
          requirements, bool(_renderer), _window.state()};
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
