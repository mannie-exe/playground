#include <platform/sdl/UISession.hpp>

namespace playground::sdl {

void UISession::synchronize(platform::WindowMetrics metrics,
                            const platform::ViewportProps &props) {
  auto mapping = platform::resolveViewport(props, metrics);
  _metrics = metrics;
  _mapping = mapping;
  _environment.viewport = mapping.logicalSize;
  _environment.pixelScale = mapping.pixelsPerLogical;
  _root.flushLayout(_environment);
}

EventResult UISession::handleEvent(const SDL_Event &event) {
  if (auto translated =
          toUIEvent(event, {static_cast<float>(_metrics.windowSize.x),
                            static_cast<float>(_metrics.windowSize.y)})) {
    translated->position = _mapping.toLogical(translated->position);
    if (translated->type == ui::EventType::PointerMove)
      translated->delta = _mapping.toLogicalDelta(translated->delta);
    _root.dispatch(*translated);
    return translated->propagationStopped ? EventResult::Consumed
           : translated->handled          ? EventResult::Handled
                                          : EventResult::Ignored;
  }
  return EventResult::Ignored;
}

void UISession::render(ui::PaintContext &context) {
  ui::PaintScope scope{context};
  context.clip({{},
                {static_cast<float>(_metrics.windowSize.x),
                 static_cast<float>(_metrics.windowSize.y)}});
  context.translate(_mapping.offset);
  context.transform(math::Transform2D::scaling(_mapping.windowUnitsPerLogical));
  context.clip({{}, _mapping.logicalSize});
  _root.flushLayout(_environment);
  _root.prepare(
      {.pixelScale = context.pixelScale(), .images = context.imagePreparer()});
  _root.render(context);
}

} // namespace playground::sdl
