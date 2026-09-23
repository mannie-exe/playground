#include <platform/Presentation.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace playground::platform {
namespace {
void positive(float value, const char *message) {
  if (!std::isfinite(value) || value <= 0)
    throw std::invalid_argument(message);
}
} // namespace

void WindowPreferences::validate() const {
  if (mode < WindowMode::Windowed || mode > WindowMode::BorderlessWorkArea ||
      display.selection < DisplaySelection::Primary ||
      display.selection > DisplaySelection::Named ||
      (display.selection == DisplaySelection::Named && display.name.empty()) ||
      !math::hasArea(exclusiveSize) || !std::isfinite(refreshRate) ||
      refreshRate < 0)
    throw std::invalid_argument("Invalid window preferences");
}
void AppViewPolicy::validate() const {
  if (initialSizing < InitialWindowSizing::Preferred ||
      initialSizing > InitialWindowSizing::RestorePrevious ||
      (preferredWindowSize && !math::hasArea(*preferredWindowSize)) ||
      !math::isFinite(minimumSize) || !math::hasArea(minimumSize))
    throw std::invalid_argument("Invalid app view policy");
}
math::Vec2i AppViewPolicy::initialWindowSize(math::Vec2i bootstrap) const {
  validate();
  if (!math::hasArea(bootstrap))
    throw std::invalid_argument("Bootstrap window size must be positive");
  return preferredWindowSize.value_or(bootstrap);
}
void ViewportProps::validate() const {
  positive(scale, "UI scale must be finite and positive");
  if (mode < ViewportMode::Reflow || mode > ViewportMode::FixedCanvas ||
      fit < ViewportFit::Contain || fit > ViewportFit::Stretch ||
      !math::isFinite(canvasSize) || !math::hasArea(canvasSize) ||
      !math::isFinite(alignment) || alignment.x < 0 || alignment.x > 1 ||
      alignment.y < 0 || alignment.y > 1)
    throw std::invalid_argument("Invalid viewport props");
}
void RenderSettings::validate() const {
  positive(resolutionScale, "Render scale must be finite and positive");
  if (resolutionScale > 4)
    throw std::invalid_argument("Render scale exceeds supported 4x limit");
}
math::Vec2i RenderSettings::targetSize(math::Vec2i drawable) const {
  validate();
  if (!math::hasArea(drawable))
    throw std::invalid_argument("Render target must have positive dimensions");
  const auto axis = [&](int extent) {
    const double result =
        std::ceil(static_cast<double>(extent) * resolutionScale);
    if (result > std::numeric_limits<int>::max())
      throw std::overflow_error("Render target extent overflow");
    return std::max(1, static_cast<int>(result));
  };
  return {axis(drawable.x), axis(drawable.y)};
}
void PresentationProps::validate() const {
  window.validate();
  viewport.validate();
  render.validate();
}
math::Vec2f uiWindowScale(const ViewportProps &props,
                          const WindowMetrics &metrics) {
  props.validate();
  positive(metrics.displayScale, "Display scale must be finite and positive");
  const float content =
      props.scale * (props.followSystemScale ? metrics.displayScale : 1);
  const auto axis = [&](int window, int pixels) {
    const double density =
        window > 0 && pixels > 0 ? static_cast<double>(pixels) / window : 1;
    const float result = static_cast<float>(
        props.followSystemScale ? content / density : props.scale);
    positive(result, "UI coordinate scale overflow");
    return result;
  };
  return {axis(metrics.windowSize.x, metrics.drawableSize.x),
          axis(metrics.windowSize.y, metrics.drawableSize.y)};
}
ViewportMapping resolveViewport(const ViewportProps &props,
                                const WindowMetrics &metrics) {
  const auto uiScale = uiWindowScale(props, metrics);
  const math::Size2 window{
      static_cast<float>(std::max(0, metrics.windowSize.x)),
      static_cast<float>(std::max(0, metrics.windowSize.y))};
  ViewportMapping result{
      .logicalSize = {window.width / uiScale.x, window.height / uiScale.y},
      .windowUnitsPerLogical = uiScale};
  if (props.mode == ViewportMode::FixedCanvas) {
    result.logicalSize = props.canvasSize;
    if (math::hasArea(window)) {
      auto factor = math::Vec2f{window.width / props.canvasSize.width,
                                window.height / props.canvasSize.height};
      if (props.fit != ViewportFit::Stretch) {
        const float uniform = props.fit == ViewportFit::Contain
                                  ? std::min(factor.x, factor.y)
                                  : std::max(factor.x, factor.y);
        factor = {uniform, uniform};
      }
      result.windowUnitsPerLogical = factor;
      result.offset = {(window.width - props.canvasSize.width * factor.x) *
                           props.alignment.x,
                       (window.height - props.canvasSize.height * factor.y) *
                           props.alignment.y};
    }
  }
  const auto density =
      math::Vec2f{window.width > 0 && metrics.drawableSize.x > 0
                      ? metrics.drawableSize.x / window.width
                      : 1,
                  window.height > 0 && metrics.drawableSize.y > 0
                      ? metrics.drawableSize.y / window.height
                      : 1};
  result.pixelsPerLogical = result.windowUnitsPerLogical * density;
  if (!math::isFinite(result.logicalSize) || !math::isFinite(result.offset) ||
      !math::isFinite(result.pixelsPerLogical) ||
      !math::hasArea(result.pixelsPerLogical))
    throw std::overflow_error("Viewport mapping overflow");
  return result;
}
math::Point2 ViewportMapping::toLogical(math::Point2 position) const {
  return {(position.x - offset.x) / windowUnitsPerLogical.x,
          (position.y - offset.y) / windowUnitsPerLogical.y};
}
math::Vec2f ViewportMapping::toLogicalDelta(math::Vec2f delta) const {
  return delta / windowUnitsPerLogical;
}
std::string_view toString(WindowMode mode) {
  switch (mode) {
  case WindowMode::Windowed:
    return "windowed";
  case WindowMode::Maximized:
    return "maximized";
  case WindowMode::DesktopFullscreen:
    return "desktop-fullscreen";
  case WindowMode::ExclusiveFullscreen:
    return "exclusive-fullscreen";
  case WindowMode::BorderlessDisplay:
    return "borderless-display";
  case WindowMode::BorderlessWorkArea:
    return "borderless-work-area";
  }
  throw std::invalid_argument("Unknown window mode");
}
std::string_view toString(InitialWindowSizing mode) {
  switch (mode) {
  case InitialWindowSizing::Preferred:
    return "preferred";
  case InitialWindowSizing::FitContent:
    return "fit-content";
  case InitialWindowSizing::RestorePrevious:
    return "restore-previous";
  }
  throw std::invalid_argument("Unknown initial sizing policy");
}
} // namespace playground::platform
