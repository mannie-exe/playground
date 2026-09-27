#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFailure.hpp>
#include <runtime/UpdateClock.hpp>
#include <support/Transaction.hpp>

namespace playground::app {

// These are the host's sequencing operations, not alternative app callbacks.
// Native window/storage work is supplied at the boundary, so the same order
// can be verified without creating a desktop window or reading user settings.
template <class Callback>
void suppressCommands(bool &suppressed, Callback &&callback) {
  const bool saved = std::exchange(suppressed, true);

  struct Restore {
    bool &suppressed;
    bool saved;

    ~Restore() { suppressed = saved; }
  } restore{suppressed, saved};

  callback();
}

template <class App, class Prepare, class Activate, class Restore,
          class Cleanup>
void activateApp(std::unique_ptr<App> &active, std::unique_ptr<App> candidate,
                 Prepare &&prepare, Activate &&activate, Restore &&restore,
                 Cleanup &&cleanup) {
  static_assert(std::is_nothrow_invocable_v<Cleanup &, App &>);
  if (!candidate)
    throw std::invalid_argument("App activation requires a candidate");
  auto previous = std::move(active);
  withRestoration(
      [&] {
        prepare();
        active = std::move(candidate);
        activate();
      },
      [&] {
        if (active)
          cleanup(*active);
        active = std::move(previous);
        restore();
      });
  if (previous)
    cleanup(*previous);
}

template <class ReleaseWindow, class Factory>
void replaceBackend(std::unique_ptr<rendering::RenderBackend> &backend,
                    ReleaseWindow &&releaseWindow, Factory &&factory) {
  if (backend)
    backend->invalidate();
  backend.reset();
  releaseWindow();
  auto candidate = factory();
  if (!candidate)
    throw std::runtime_error("Backend factory returned no renderer");
  backend = std::move(candidate);
}

template <class ReleaseWindow, class Factory, class Notify>
void recoverBackend(std::unique_ptr<rendering::RenderBackend> &backend,
                    rendering::RecoveryState &recovery,
                    runtime::UpdateClock &clock, std::string reason,
                    ReleaseWindow &&releaseWindow, Factory &&factory,
                    Notify &&notify) {
  if (!recovery.begin(std::move(reason)))
    throw rendering::RenderFailure(
        "Renderer recovery attempt budget exhausted: " + recovery.reason());
  try {
    replaceBackend(backend, releaseWindow, factory);
    recovery.recovered();
    clock.rebase();
    notify();
  } catch (...) {
    recovery.failed();
    throw;
  }
}

template <class Render, class BeforePresent>
rendering::PresentationOutcome renderFrame(rendering::RenderBackend &backend,
                                           rendering::RenderFrameProps props,
                                           Render &&render,
                                           BeforePresent &&beforePresent) {
  auto frame = backend.beginFrame(props);
  if (frame)
    render(*frame);
  beforePresent();
  return frame ? frame->present() : rendering::PresentationOutcome::Skipped;
  // RAII destroys the frame on success and on every exception, before recovery.
}

} // namespace playground::app
