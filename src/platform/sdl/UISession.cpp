#include <chrono>
#include <stdexcept>

#include <platform/sdl/UISession.hpp>
#include <platform/sdl/WindowServices.hpp>
#include <support/PerformanceMonitor.hpp>

namespace playground::sdl {
runtime::ActivityDemand UISession::activityDemand() {
  if (_timing != UISessionTiming::Monotonic)
    throw std::logic_error(
        "UI activity deadlines require monotonic session timing");
  runtime::ActivityDemand result{.update = _root.needsUpdate(),
                                 .paint = _root.needsPaint() ||
                                          _root.motion().needsFrame()};
  if (auto delay = _root.nextUpdateDelay()) {
    const auto remaining =
        runtime::ActivityClock::time_point::max() - _lastUpdate;
    result.wakeAt =
        *delay >= std::chrono::duration<double>(remaining).count()
            ? runtime::ActivityClock::time_point::max()
            : _lastUpdate +
                  std::chrono::duration_cast<runtime::ActivityClock::duration>(
                      std::chrono::duration<double>{*delay});
  }
  return result;
}

void UISession::synchronize(platform::WindowMetrics metrics,
                            const platform::ViewportProps &props,
                            PerformanceMonitor *monitor) {
  _monitor = monitor;
  _root.setTimingEnabled(monitor && monitor->isEnabled());
  auto mapping = platform::resolveViewport(props, metrics);
  _metrics = metrics;
  _mapping = mapping;
  _environment.viewport = mapping.logicalSize;
  _environment.pixelScale = mapping.pixelsPerLogical;
  _root.flushLayout(_environment);
  reportWork();
}

void UISession::reportWork() {
  const auto now = _root.workSample();
  auto delta = now;
  delta.work = ui::workDelta(now.work, _reported.work);
  for (std::size_t i = 0; i < delta.milliseconds.size(); ++i)
    delta.milliseconds[i] -= _reported.milliseconds[i];
  if (_monitor)
    _monitor->recordUI(delta);
  _reported = now;
}

void UISession::update(float seconds) {
  const auto now = runtime::ActivityClock::now();
  const double elapsed =
      _timing == UISessionTiming::Monotonic
          ? std::chrono::duration<double>(now - _lastUpdate).count()
          : seconds;
  _lastUpdate = now;
  _root.update(elapsed);
  if (_windowServices)
    _windowServices->pump();
  _root.flushLayout(_environment);
  if (_windowServices)
    _windowServices->publish(_root, _mapping);
  reportWork();
}

EventResult UISession::handleEvent(const SDL_Event &event) {
  if (_timing == UISessionTiming::Monotonic)
    update(0);
  if (auto translated =
          toUIEvent(event, {static_cast<float>(_metrics.windowSize.x),
                            static_cast<float>(_metrics.windowSize.y)})) {
    translated->position = _mapping.toLogical(translated->position);
    if (translated->type == ui::EventType::PointerMove)
      translated->delta = _mapping.toLogicalDelta(translated->delta);
    _root.dispatch(*translated);
    _root.flushLayout(_environment);
    if (_windowServices)
      _windowServices->publish(_root, _mapping);
    reportWork();
    return translated->propagationStopped ? EventResult::Consumed
           : translated->handled          ? EventResult::Handled
                                          : EventResult::Ignored;
  }
  return EventResult::Ignored;
}

void UISession::render(rendering::PaintContext &context,
                       scene::SceneRenderer *scenes) {
  if (_timing == UISessionTiming::Monotonic)
    update(0);
  _root.motion().sample();
  rendering::PaintScope scope{context};
  context.clip({{},
                {static_cast<float>(_metrics.windowSize.x),
                 static_cast<float>(_metrics.windowSize.y)}});
  context.translate(_mapping.offset);
  context.transform(math::Transform2D::scaling(_mapping.windowUnitsPerLogical));
  context.clip({{}, _mapping.logicalSize});
  _root.flushLayout(_environment);
  _root.prepare({.pixelScale = context.pixelScale(),
                 .images = context.imagePreparer(),
                 .scenes = scenes,
                 .text = context.textPreparer(),
                 .graphics = &_graphics});
  _root.render(context);
  if (_windowServices)
    _windowServices->publish(_root, _mapping);
  reportWork();
}

} // namespace playground::sdl
