#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <math/Geometry2D.hpp>
#include <rendering/RenderSettings.hpp>
#include <rendering/RendererTypes.hpp>
#include <ui/Interaction.hpp>
#include <ui/Theme.hpp>

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
  ui::ColorSchemePreference colorScheme{ui::ColorSchemePreference::System};
  // Resolved by settings; applications should leave user contrast inherited.
  ui::ContrastPreference userContrast{ui::ContrastPreference::System};
  InitialWindowSizing initialSizing{InitialWindowSizing::Preferred};
  std::optional<math::Vec2i> preferredWindowSize;
  math::Size2 minimumSize{1, 1};
  bool resizable{true};
  ui::InteractionProps interaction;
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

struct PresentationProps {
  WindowPreferences window;
  ViewportProps viewport;
  rendering::RenderSettings render;
  rendering::RendererPreferences renderer;
  void validate() const;
  bool operator==(const PresentationProps &) const = default;
};

struct WindowMetrics {
  math::Vec2i windowSize;
  math::Vec2i drawableSize;
  float displayScale{1};
};

struct ViewportMapping {
  bool operator==(const ViewportMapping &) const = default;
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
