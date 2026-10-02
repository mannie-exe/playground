#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>

#include <rendering/PaintImage.hpp>
#include <rendering/RenderRuntime.hpp>
#include <rendering/RenderSettings.hpp>
#include <rendering/RendererTypes.hpp>
#include <runtime/MotionPreference.hpp>

namespace playground::rendering {

// Stored preferences, including explicitly inactive future features. Capability
// reporting determines effectiveness; accepting a value does not implement it.
enum class QualityLevel { Off, Low, Medium, High, Ultra };
enum class Antialiasing { None, MSAA2, MSAA4, MSAA8, Temporal };

struct Graphics2D {
  float rasterScale{1}; // opted-in cached content, never ordinary text/layout
  Sampling sampling{Sampling::Linear};
  bool operator==(const Graphics2D &) const = default;
};

struct Graphics3D {
  float resolutionScale{1};
  Sampling reconstruction{Sampling::Linear};
  QualityLevel textures{QualityLevel::High}, meshes{QualityLevel::High};
  QualityLevel shadows{QualityLevel::High}, effects{QualityLevel::High};
  Antialiasing antialiasing{Antialiasing::None};
  unsigned anisotropy{1};
  bool ambientOcclusion{}, reflections{};
  bool operator==(const Graphics3D &) const = default;
};

struct QualityPolicy {
  bool enabled{};
  bool sceneResolution{true};
  float minimumSceneScale{0.5f};
  double targetFramesPerSecond{60};
  bool operator==(const QualityPolicy &) const = default;
};

struct GraphicsSettings {
  runtime::MotionPreference motion{runtime::MotionPreference::System};
  RenderSettings presentation;
  RendererPreferences renderer;
  Graphics2D twoD;
  Graphics3D threeD;
  QualityPolicy automatic;
  FramePacingProps pacing;
  ResourceBudgetProps budgets;
  void validate() const;
  bool operator==(const GraphicsSettings &) const = default;
};

enum class GraphicsGroup { Presentation, TwoD, ThreeD, Automatic, Resources };
enum class SettingKind { Number, Integer, Boolean };

struct GraphicsSetting {
  std::string_view key, label, unit;
  GraphicsGroup group;
  SettingKind kind;
  double minimum, maximum, step;
  bool inactive;
  std::span<const std::string_view> choices;
  double (*get)(const GraphicsSettings &);
  void (*set)(GraphicsSettings &, double);
};

std::span<const GraphicsSetting> graphicsSettingsSchema();
void setGraphicsSetting(GraphicsSettings &, const GraphicsSetting &, double);

struct ResolvedGraphicsState {
  GraphicsSettings requested;
  float sceneScale{1};
  std::uint64_t revision{1};
  std::string reason{"Manual settings"};
  bool timingAvailable{};
  bool atMinimum{};
  std::optional<float> appliedSceneScale;
  bool pending{true};
};

// One owner-thread coordinator; all participating views share one quality
// scale. Per-view samples use an equal share of the scene budget. This
// conservative heuristic is not a measurement of summed/critical-path GPU frame
// duration.
class QualityController {
  ResolvedGraphicsState _state;
  ResourceDomainId _domain;
  std::map<std::uint64_t, std::uint64_t> _views;
  std::uint64_t _lastFrame{}, _observations{};
  unsigned _slow{}, _fast{};
  double _updateMilliseconds{};
  bool change(float scale, std::string reason);

public:
  const ResolvedGraphicsState &state() const noexcept { return _state; }

  void configure(GraphicsSettings settings);
  void reset(ResourceDomainId domain, bool timingAvailable);
  bool observe(const GPUTimingSample &sample,
               const FramePacingProps *pacing = nullptr);
  void observeCPU(const CPUSample &sample);
  bool pressure();
};
} // namespace playground::rendering
