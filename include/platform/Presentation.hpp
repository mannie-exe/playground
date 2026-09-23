#pragma once

#include <math/Geometry2D.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace playground::platform {

enum class WindowMode {
  Windowed,
  Maximized,
  DesktopFullscreen,
  ExclusiveFullscreen,
  BorderlessDisplay,
  BorderlessWorkArea
};
enum class DisplaySelection { Primary, Current, Named };
enum class InitialWindowSizing { Preferred, FitContent, RestorePrevious };
enum class ViewportMode { Reflow, FixedCanvas };
enum class ViewportFit { Contain, Cover, Stretch };

struct DisplayPreference {
  DisplaySelection selection{DisplaySelection::Current};
  std::string name;
  bool operator==(const DisplayPreference &) const = default;
};

struct WindowPreferences {
  WindowMode mode{WindowMode::Windowed};
  DisplayPreference display;
  bool decorated{true};
  bool center{true};
  math::Vec2i exclusiveSize{1920, 1080};
  float refreshRate{};
  void validate() const;
  bool operator==(const WindowPreferences &) const = default;
};

struct AppViewPolicy {
  InitialWindowSizing initialSizing{InitialWindowSizing::Preferred};
  std::optional<math::Vec2i> preferredWindowSize;
  math::Size2 minimumSize{1, 1};
  bool resizable{true};
  void validate() const;
  math::Vec2i initialWindowSize(math::Vec2i bootstrap) const;
};

struct ViewportProps {
  ViewportMode mode{ViewportMode::Reflow};
  ViewportFit fit{ViewportFit::Contain};
  math::Size2 canvasSize{800, 600};
  math::Vec2f alignment{0.5f, 0.5f};
  float scale{1};
  bool followSystemScale{true};
  void validate() const;
  bool operator==(const ViewportProps &) const = default;
};

struct RenderSettings {
  // Whole-frame resolution; unlike UI scale, this never changes layout/input.
  float resolutionScale{1};
  void validate() const;
  math::Vec2i targetSize(math::Vec2i drawable) const;
  bool operator==(const RenderSettings &) const = default;
};

struct PresentationProps {
  WindowPreferences window;
  ViewportProps viewport;
  RenderSettings render;
  void validate() const;
  bool operator==(const PresentationProps &) const = default;
};

struct WindowMetrics {
  math::Vec2i windowSize;
  math::Vec2i drawableSize;
  float displayScale{1};
};

struct ViewportMapping {
  math::Size2 logicalSize;
  math::Vec2f windowUnitsPerLogical{1, 1};
  math::Vec2f offset;
  math::Vec2f pixelsPerLogical{1, 1};
  math::Point2 toLogical(math::Point2 windowPosition) const;
  math::Vec2f toLogicalDelta(math::Vec2f windowDelta) const;
};

ViewportMapping resolveViewport(const ViewportProps &, const WindowMetrics &);
math::Vec2f uiWindowScale(const ViewportProps &, const WindowMetrics &);

std::string_view toString(WindowMode);
std::string_view toString(InitialWindowSizing);

} // namespace playground::platform
