#include <algorithm>
#include <cmath>

#include <rendering/GraphicsSettings.hpp>

namespace playground::rendering {
void GraphicsSettings::validate() const {
  if (static_cast<unsigned>(motion) > 3)
    throw std::invalid_argument("Invalid motion preference");
  presentation.validate();
  renderer.validate();
  pacing.validate();
  budgets.validate();
  const auto scale = [](float v) {
    return std::isfinite(v) && v >= .25f && v <= 2;
  };
  const auto quality = [](QualityLevel v) {
    return static_cast<unsigned>(v) <= 4;
  };
  const auto sampling = [](Sampling v) {
    return v == Sampling::Nearest || v == Sampling::Linear;
  };
  if (!scale(twoD.rasterScale) || !scale(threeD.resolutionScale) ||
      !scale(automatic.minimumSceneScale) ||
      (automatic.enabled &&
       automatic.minimumSceneScale > threeD.resolutionScale) ||
      !std::isfinite(automatic.targetFramesPerSecond) ||
      automatic.targetFramesPerSecond < 1 ||
      automatic.targetFramesPerSecond > 1000 || !sampling(twoD.sampling) ||
      !sampling(threeD.reconstruction) || !quality(threeD.textures) ||
      !quality(threeD.meshes) || !quality(threeD.shadows) ||
      !quality(threeD.effects) ||
      static_cast<unsigned>(threeD.antialiasing) > 4 ||
      (threeD.anisotropy != 1 && threeD.anisotropy != 2 &&
       threeD.anisotropy != 4 && threeD.anisotropy != 8 &&
       threeD.anisotropy != 16))
    throw std::invalid_argument(
        "Invalid graphics quality settings or adjustment bounds");
}

void QualityController::configure(GraphicsSettings settings) {
  settings.validate();
  _state.requested = std::move(settings);
  _state.sceneScale = _state.requested.threeD.resolutionScale;
  ++_state.revision;
  _state.pending = true;
  _state.reason = _state.requested.automatic.enabled
                      ? "Automatic: awaiting measurements"
                      : "Manual settings";
  _state.atMinimum = false;
  _slow = _fast = 0;
  _updateMilliseconds = 0;
  _views.clear();
  _lastFrame = _observations = 0;
}

void QualityController::reset(ResourceDomainId domain, bool timingAvailable) {
  _domain = domain;
  _state.timingAvailable = timingAvailable;
  _state.appliedSceneScale.reset();
  configure(_state.requested);
  if (_state.requested.automatic.enabled && !timingAvailable)
    _state.reason = "GPU timing unavailable; memory-pressure adjustment only";
}

bool QualityController::change(float scale, std::string reason) {
  scale = std::clamp(scale, _state.requested.automatic.minimumSceneScale,
                     _state.requested.threeD.resolutionScale);
  _state.atMinimum = scale <= _state.requested.automatic.minimumSceneScale;
  _state.reason = std::move(reason);
  if (std::abs(scale - _state.sceneScale) < .001f)
    return false;
  _state.sceneScale = scale;
  ++_state.revision;
  _state.pending = true;
  _slow = _fast = 0;
  _observations = 0;
  return true;
}

void QualityController::observeCPU(const CPUSample &sample) {
  const auto index = static_cast<std::size_t>(CPUPhase::Update);
  if (sample.measured[index] && std::isfinite(sample.milliseconds[index]))
    _updateMilliseconds =
        .9 * _updateMilliseconds + .1 * sample.milliseconds[index];
}

bool QualityController::observe(const GPUTimingSample &sample,
                                const FramePacingProps *pacing) {
  if (_state.timingAvailable && sample.domain == _domain &&
      sample.label == "scene3d" && sample.context.workloadId &&
      sample.context.frameId && std::isfinite(sample.milliseconds) &&
      sample.milliseconds >= 0 &&
      sample.context.qualityRevision == _state.revision) {
    _state.appliedSceneScale = _state.sceneScale;
    _state.pending = false;
  }
  const auto &p = _state.requested.automatic;
  if (!p.enabled || !p.sceneResolution || !_state.timingAvailable ||
      sample.domain != _domain || sample.label != "scene3d" ||
      !sample.context.workloadId || !sample.context.frameId ||
      sample.context.qualityRevision != _state.revision ||
      !std::isfinite(sample.milliseconds) || sample.milliseconds < 0)
    return false;
  const auto frame = sample.context.frameId;
  if (frame < _lastFrame)
    return false;
  _views[sample.context.workloadId] = frame;
  std::erase_if(_views, [frame](const auto &v) {
    return frame > v.second && frame - v.second > 120;
  });
  double target = p.targetFramesPerSecond;
  if (auto cap =
          (pacing ? *pacing : _state.requested.pacing).maximumFramesPerSecond)
    target = std::min(target, *cap);
  if (_updateMilliseconds > 1000.0 / target) {
    _slow = _fast = 0;
    _state.reason =
        "CPU update exceeds target; scene timing adjustment suspended";
    return false;
  }
  const auto budget = 850.0 / target / std::max<std::size_t>(1, _views.size());
  if (frame != _lastFrame) {
    _lastFrame = frame;
    ++_observations;
  }
  if (sample.milliseconds > budget * 1.08) {
    ++_slow;
    _fast = 0;
  } else if (sample.milliseconds < budget * .75) {
    ++_fast;
    _slow = 0;
  } else
    _slow = _fast = 0;
  if (_observations >= 8 && _slow >= 3)
    return change(_state.sceneScale - .05f,
                  "Scene GPU work exceeds its time allowance");
  if (_observations >= 90 && _fast >= 60)
    return change(_state.sceneScale + .025f, "Sustained scene GPU headroom");
  return false;
}

bool QualityController::pressure() {
  if (!_state.requested.automatic.enabled ||
      !_state.requested.automatic.sceneResolution)
    return false;
  return change(_state.sceneScale - .1f, "Managed allocation pressure");
}
} // namespace playground::rendering

namespace playground::rendering {
namespace {
constexpr std::string_view rendererNames[]{"auto", "software", "sdl-gpu"};
constexpr std::string_view driverNames[]{"auto", "vulkan"};
constexpr std::string_view samplingNames[]{"nearest", "linear"};
constexpr std::string_view qualityNames[]{"off", "low", "medium", "high",
                                          "ultra"};
constexpr std::string_view aaNames[]{"none", "msaa-2x", "msaa-4x", "msaa-8x",
                                     "temporal"};
constexpr std::string_view anisoNames[]{"1x", "2x", "4x", "8x", "16x"};
constexpr std::string_view motionNames[]{"system", "full", "reduced", "none"};
const GraphicsSetting schema[]{
    {"motion", "UI motion", "", GraphicsGroup::Presentation,
     SettingKind::Integer, 0, 3, 1, false, motionNames,
     [](const GraphicsSettings &s) { return static_cast<double>(s.motion); },
     [](GraphicsSettings &s, double v) {
       s.motion = static_cast<runtime::MotionPreference>(v);
     }},
    {"renderer", "Renderer preference", "", GraphicsGroup::Presentation,
     SettingKind::Integer, 0, 2, 1, false, rendererNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.renderer.backend);
     },
     [](GraphicsSettings &s, double v) {
       s.renderer.backend = static_cast<RendererChoice>(v);
     }},
    {"gpu_driver", "GPU driver preference", "", GraphicsGroup::Presentation,
     SettingKind::Integer, 0, 1, 1, false, driverNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.renderer.driver);
     },
     [](GraphicsSettings &s, double v) {
       s.renderer.driver = static_cast<GPUDriver>(v);
     }},
    {"renderer_fallback",
     "Allow compatible renderer fallback",
     "",
     GraphicsGroup::Presentation,
     SettingKind::Boolean,
     0,
     1,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.renderer.allowFallback);
     },
     [](GraphicsSettings &s, double v) { s.renderer.allowFallback = bool(v); }},
    {"frame_scale",
     "Whole-frame resolution",
     "%",
     GraphicsGroup::Presentation,
     SettingKind::Number,
     1,
     400,
     5,
     false,
     {},
     [](const GraphicsSettings &s) {
       return 100.0 * s.presentation.resolutionScale;
     },
     [](GraphicsSettings &s, double v) {
       s.presentation.resolutionScale = static_cast<float>(v / 100);
     }},
    {"vsync",
     "Vertical synchronization",
     "",
     GraphicsGroup::Presentation,
     SettingKind::Boolean,
     0,
     1,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.presentation.vsync);
     },
     [](GraphicsSettings &s, double v) {
       s.presentation.vsync = static_cast<bool>(v);
     }},
    {"glyph_atlases",
     "Glyph atlases",
     "",
     GraphicsGroup::Presentation,
     SettingKind::Boolean,
     0,
     1,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.presentation.glyphAtlases);
     },
     [](GraphicsSettings &s, double v) {
       s.presentation.glyphAtlases = static_cast<bool>(v);
     }},
    {"frame_cap",
     "Presentation cap (0: uncapped)",
     "FPS",
     GraphicsGroup::Presentation,
     SettingKind::Number,
     0,
     1000,
     5,
     false,
     {},
     [](const GraphicsSettings &s) {
       return s.pacing.maximumFramesPerSecond.value_or(0);
     },
     [](GraphicsSettings &s, double v) {
       s.pacing.maximumFramesPerSecond =
           v == 0 ? std::nullopt : std::optional<double>{v};
     }},
    {"raster_scale",
     "Eligible 2D cache resolution",
     "%",
     GraphicsGroup::TwoD,
     SettingKind::Number,
     25,
     200,
     5,
     false,
     {},
     [](const GraphicsSettings &s) { return 100.0 * s.twoD.rasterScale; },
     [](GraphicsSettings &s, double v) {
       s.twoD.rasterScale = static_cast<float>(v / 100);
     }},
    {"image_filter", "2D image reconstruction", "", GraphicsGroup::TwoD,
     SettingKind::Integer, 0, 1, 1, false, samplingNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.twoD.sampling);
     },
     [](GraphicsSettings &s, double v) {
       s.twoD.sampling = static_cast<Sampling>(v);
     }},
    {"scene_scale",
     "3D scene resolution",
     "%",
     GraphicsGroup::ThreeD,
     SettingKind::Number,
     25,
     200,
     5,
     false,
     {},
     [](const GraphicsSettings &s) { return 100.0 * s.threeD.resolutionScale; },
     [](GraphicsSettings &s, double v) {
       s.threeD.resolutionScale = static_cast<float>(v / 100);
     }},
    {"scene_filter", "Scene reconstruction", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 1, 1, false, samplingNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.reconstruction);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.reconstruction = static_cast<Sampling>(v);
     }},
    {"texture_quality", "Texture quality", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, qualityNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.textures);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.textures = static_cast<QualityLevel>(v);
     }},
    {"mesh_quality", "Mesh detail", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, qualityNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.meshes);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.meshes = static_cast<QualityLevel>(v);
     }},
    {"shadow_quality", "Shadow quality", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, qualityNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.shadows);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.shadows = static_cast<QualityLevel>(v);
     }},
    {"effect_quality", "Effects quality", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, qualityNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.effects);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.effects = static_cast<QualityLevel>(v);
     }},
    {"antialiasing", "Antialiasing", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, aaNames,
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.antialiasing);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.antialiasing = static_cast<Antialiasing>(v);
     }},
    {"anisotropy", "Anisotropic filtering", "", GraphicsGroup::ThreeD,
     SettingKind::Integer, 0, 4, 1, true, anisoNames,
     [](const GraphicsSettings &s) { return std::log2(s.threeD.anisotropy); },
     [](GraphicsSettings &s, double v) {
       s.threeD.anisotropy = 1u << static_cast<unsigned>(v);
     }},
    {"ambient_occlusion",
     "Ambient occlusion",
     "",
     GraphicsGroup::ThreeD,
     SettingKind::Boolean,
     0,
     1,
     1,
     true,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.ambientOcclusion);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.ambientOcclusion = static_cast<bool>(v);
     }},
    {"reflections",
     "Reflections",
     "",
     GraphicsGroup::ThreeD,
     SettingKind::Boolean,
     0,
     1,
     1,
     true,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.threeD.reflections);
     },
     [](GraphicsSettings &s, double v) {
       s.threeD.reflections = static_cast<bool>(v);
     }},
    {"automatic",
     "Automatic quality",
     "",
     GraphicsGroup::Automatic,
     SettingKind::Boolean,
     0,
     1,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.automatic.enabled);
     },
     [](GraphicsSettings &s, double v) {
       s.automatic.enabled = static_cast<bool>(v);
     }},
    {"adaptive_scene",
     "Allow scene-resolution changes",
     "",
     GraphicsGroup::Automatic,
     SettingKind::Boolean,
     0,
     1,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.automatic.sceneResolution);
     },
     [](GraphicsSettings &s, double v) {
       s.automatic.sceneResolution = static_cast<bool>(v);
     }},
    {"minimum_scene_scale",
     "Minimum scene resolution",
     "%",
     GraphicsGroup::Automatic,
     SettingKind::Number,
     25,
     200,
     5,
     false,
     {},
     [](const GraphicsSettings &s) {
       return 100.0 * s.automatic.minimumSceneScale;
     },
     [](GraphicsSettings &s, double v) {
       s.automatic.minimumSceneScale = static_cast<float>(v / 100);
     }},
    {"target_fps",
     "Quality performance target",
     "FPS",
     GraphicsGroup::Automatic,
     SettingKind::Number,
     1,
     1000,
     5,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.automatic.targetFramesPerSecond);
     },
     [](GraphicsSettings &s, double v) {
       s.automatic.targetFramesPerSecond = static_cast<double>(v);
     }},
    {"outstanding_frames",
     "Outstanding frames",
     "frames",
     GraphicsGroup::Resources,
     SettingKind::Integer,
     1,
     3,
     1,
     false,
     {},
     [](const GraphicsSettings &s) {
       return static_cast<double>(s.pacing.maxOutstandingFrames);
     },
     [](GraphicsSettings &s, double v) {
       s.pacing.maxOutstandingFrames = static_cast<unsigned>(v);
     }},
    {"cpu_mib",
     "Managed CPU ceiling",
     "MiB",
     GraphicsGroup::Resources,
     SettingKind::Number,
     1.0 / 1048576,
     65536,
     64,
     false,
     {},
     [](const GraphicsSettings &s) { return s.budgets.cpuBytes / 1048576.0; },
     [](GraphicsSettings &s, double v) {
       s.budgets.cpuBytes = static_cast<std::size_t>(v * 1048576);
     }},
    {"gpu_mib",
     "Managed GPU ceiling",
     "MiB",
     GraphicsGroup::Resources,
     SettingKind::Number,
     1.0 / 1048576,
     65536,
     64,
     false,
     {},
     [](const GraphicsSettings &s) { return s.budgets.gpuBytes / 1048576.0; },
     [](GraphicsSettings &s, double v) {
       s.budgets.gpuBytes = static_cast<std::size_t>(v * 1048576);
     }},
    {"target_mib",
     "GPU target ceiling",
     "MiB",
     GraphicsGroup::Resources,
     SettingKind::Number,
     1.0 / 1048576,
     65536,
     64,
     false,
     {},
     [](const GraphicsSettings &s) {
       return s.budgets.targetBytes / 1048576.0;
     },
     [](GraphicsSettings &s, double v) {
       s.budgets.targetBytes = static_cast<std::size_t>(v * 1048576);
     }},
    {"preparation_mib",
     "Preparation ceiling",
     "MiB",
     GraphicsGroup::Resources,
     SettingKind::Number,
     1.0 / 1048576,
     65536,
     64,
     false,
     {},
     [](const GraphicsSettings &s) {
       return s.budgets.preparationBytes / 1048576.0;
     },
     [](GraphicsSettings &s, double v) {
       s.budgets.preparationBytes = static_cast<std::size_t>(v * 1048576);
     }},
};
} // namespace

std::span<const GraphicsSetting> graphicsSettingsSchema() { return schema; }

void setGraphicsSetting(GraphicsSettings &s, const GraphicsSetting &field,
                        double v) {
  if (!std::isfinite(v) || v < field.minimum || v > field.maximum ||
      (field.kind != SettingKind::Number && std::floor(v) != v))
    throw std::invalid_argument("Invalid graphics setting: " +
                                std::string{field.key});
  field.set(s, v);
}
} // namespace playground::rendering
