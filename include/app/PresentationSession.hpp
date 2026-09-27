#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_events.h>

#include <app/AppTypes.hpp>
#include <platform/Window.hpp>
#include <platform/sdl/WindowServices.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderBackendProps.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/UpdateClock.hpp>

namespace playground::app {

// Native presentation ownership; no app lifetime or settings-store ownership.
class PresentationSession {
  Window _window;
  sdl::WindowServices _windowServices;
  const rendering::RenderBackendProps _backendProps;
  const math::Vec2i _bootstrapSize;
  std::unique_ptr<rendering::RenderBackend> _renderer;
  rendering::RendererState _rendererState;
  rendering::RecoveryState _rendererRecovery;

  AppWindowProps _windowProps;
  platform::AppViewPolicy _viewPolicy;
  platform::PresentationProps _presentation;

public:
  using Measure =
      std::function<std::optional<math::Size2>(math::Size2, math::Vec2f)>;

  struct Checkpoint {
    AppWindowProps windowProps;
    platform::AppViewPolicy view;
    platform::PresentationProps presentation;
    rendering::RendererState renderer;
    rendering::RendererRequirements requirements;
    bool hasRenderer;
    WindowState window;
  };

  explicit PresentationSession(WindowConfig window,
                               rendering::RenderBackendProps backend = {});

  const WindowState &windowState() const { return _window.state(); }

  sdl::WindowServices &windowServices() { return _windowServices; }

  platform::WindowMetrics windowMetrics() const { return _window.metrics(); }

  const AppWindowProps &windowProps() const { return _windowProps; }

  const platform::AppViewPolicy &viewPolicy() const { return _viewPolicy; }

  const platform::PresentationProps &presentation() const {
    return _presentation;
  }

  const rendering::RendererState &rendererState() const {
    return _rendererState;
  }

  const rendering::RecoveryState &recovery() const { return _rendererRecovery; }

  rendering::RenderBackend *renderer() const noexcept {
    return _renderer.get();
  }

  void setWindowProps(AppWindowProps value) { _windowProps = std::move(value); }

  void setViewPolicy(platform::AppViewPolicy value) {
    value.validate();
    _viewPolicy = value;
  }

  void setPresentation(platform::PresentationProps value) {
    value.validate();
    _presentation = std::move(value);
  }

  void handleEvent(const SDL_Event &event) { _window.handleEvent(event); }

  Checkpoint checkpoint(rendering::RendererRequirements requirements);
  void restore(const Checkpoint &checkpoint);
  bool configureRenderer(rendering::RendererPreferences preferences,
                         rendering::RendererRequirements requirements);
  void recover(std::string reason, rendering::RendererRequirements requirements,
               runtime::UpdateClock &clock,
               const std::function<void()> &notify);

  void resetRecovery() { _rendererRecovery = rendering::RecoveryState{}; }

  void observeFrame(rendering::PresentationOutcome outcome, double seconds);
  void applyWindowProps();
  void prepareWindowForSizing();
  void applyViewSizing(const std::optional<platform::SavedWindow> &saved,
                       const Measure &measure);
  void fitContent(const Measure &measure);
  void applyPresentation(bool preservePosition = false);
  platform::SavedWindow savedWindow();
};

} // namespace playground::app
