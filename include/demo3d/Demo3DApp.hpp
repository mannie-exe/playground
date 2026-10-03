#pragma once

#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <app/IApp.hpp>
#include <assets/AssetCatalog.hpp>
#include <input/ViewControlSession.hpp>
#include <platform/sdl/UISession.hpp>
#include <runtime/Benchmark.hpp>
#include <runtime/Executor.hpp>
#include <scene/Animation.hpp>
#include <scene/Controllers.hpp>
#include <scene/Environment.hpp>
#include <scene/ModelImport.hpp>
#include <ui/content/InspectionView.hpp>
#include <ui/content/SceneView.hpp>
#include <ui/content/Text.hpp>
#include <world/Locomotion.hpp>

namespace playground::demo3d {
void registerAssets(assets::AssetCatalog &catalog);

enum class DemoKind { Material, Bistro, Chess, Benchmark };

class Demo3DApp final : public IApp {
  struct Resources {
    std::vector<scene::ModelHandle> models;
    scene::PreparedEnvironment environment;
    rendering::TextureHandle smoke, environmentReference;
  };

  std::shared_ptr<runtime::ResourceLedger> _ledger;
  DemoKind _kind;
  Resources _resources;
  std::shared_ptr<scene::Scene3D> _scene;
  scene::ObjectId _smoke;
  scene::Playback _playback;
  world::World _world;
  world::SpaceId _space;
  world::EntityId _scenery, _subject;
  scene::FollowCameraRig _follow;
  scene::LookController _look;
  world::CharacterFacingController _facing;
  std::optional<scene::PoseHistory> _poses;
  bool _followMode{};
  math::Vec2f _keyboardLook, _gamepadLook, _gamepadMove;
  std::optional<std::uint32_t> _gamepad;
  std::optional<input::ContextId> _gamepadContext;
  input::ControlsSettings _preferences;
  void applyControls(AppContext &);
  std::unique_ptr<scene::SceneInstanceProjection> _projection;
  scene::OrbitController _camera;
  scene::FreeCameraController _freeCamera;
  scene::FreeCameraProps _initialCamera;
  scene::CameraDirector _director;
  scene::CameraId _interactiveCamera{};
  input::ViewControlSession _controls;
  scene::CameraId _pathCamera{};
  runtime::BenchmarkRun _benchmark;
  ui::Text *_caption{};
  std::uint64_t _benchmarkSubmitted{}, _seenCPU{};
  std::set<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>> _seenGPU;
  rendering::GraphicsSettings _benchmarkGraphics;
  math::Vec2i _benchmarkPixels, _benchmarkTargetPixels;
  rendering::ResourceDomainId _benchmarkDomain;
  rendering::SceneWork _benchmarkWork;
  double _loadStarted{}, _loadedSeconds{}, _lastReport{};
  bool _benchmarkReported{};
  float _benchmarkScale{};
  std::uint64_t _measuredSubmitted{}, _finalSubmitted{}, _queryDrops{},
      _bufferDiscards{};
  void updateBenchmark(AppContext &);
  void reportBenchmark(AppContext &, bool final);
  void restartBenchmark(AppContext &);
  void prepareBenchmark(AppContext &);
  scene::CameraPath benchmarkPath() const;
  std::optional<scene::CameraPath> _tour;
  input::InputSnapshot _navigation;
  scene::WorldCamera camera() const;
  void project(ui::SceneViewProps &);
  void updateView();
  float _exposure{1};
  bool _lighting{true};

  sdl::UISession _ui{sdl::UISessionTiming::Monotonic};
  ui::SceneView *_view{};
  ui::InspectionView *_inspection{};
  ui::Connection _inspectionChanged;

  bool inspecting() const noexcept {
    return _kind == DemoKind::Material || _kind == DemoKind::Chess;
  }

  sdl::AssetPreparation _preparation;
  sdl::AssetPreparationProps _preparationProps;
  bool _loadRequested{}, _reclaimedPreparation{};
  std::optional<runtime::ActivityClock::time_point> _loadRetryAt;

  void preparationError(AppContext &, std::string_view);
  void createView(AppContext &ctx, Resources resources);
  void synchronize(AppContext &ctx);

public:
  explicit Demo3DApp(DemoKind kind = DemoKind::Material,
                     std::shared_ptr<runtime::ResourceLedger> ledger =
                         runtime::defaultResourceLedger());
  ~Demo3DApp() override;
  static AppInfo staticInfo(DemoKind kind = DemoKind::Material);

  AppInfo info() const override { return staticInfo(_kind); }

  void configureLaunch(const AppLaunchProps &) override;

  void onActivityInterrupted(AppContext &,
                             AppInterruption reason) noexcept override {
    if (_inspection)
      _inspection->cancelNavigation();
    _controls.suspend(reason == AppInterruption::Focus
                          ? input::ControlReason::Focus
                          : input::ControlReason::UI);
    _navigation = {};
    _keyboardLook = _gamepadLook = _gamepadMove = {};
    _facing.reset();
    if (_kind == DemoKind::Benchmark)
      _benchmark.invalidate(reason == AppInterruption::Focus
                                ? runtime::BenchmarkInterruption::Focus
                                : runtime::BenchmarkInterruption::Settings);
  }

  runtime::ActivityProps activityProps() const override;
  runtime::ActivityDemand activityDemand() override;

  input::InputClaims inputClaims() override { return _ui.inputClaims(); }

  input::ControlCapabilities controlCapabilities() const override {
    const bool manual = _kind != DemoKind::Benchmark;
    return {manual, manual && !inspecting(), manual && _followMode,
            manual && _followMode};
  }

  std::string_view controlRestriction() const override {
    return _kind == DemoKind::Benchmark
               ? "Benchmark playback ignores manual controls."
           : inspecting()
               ? "Inspection uses visible-pointer orbit, pan and zoom; R "
                 "resets "
                 "panning. Gamepad, capture and locomotion are inactive."
           : _followMode
               ? "Follow control uses a kinematic subject without gravity or "
                 "collision. Gamepad: Start engages the selected viewport."
               : "Free camera: F switches to follow control. Locomotion and "
                 "perspective preferences apply in follow mode. Gamepad: Start "
                 "engages the selected viewport.";
  }

  std::optional<runtime::SimulationTimingProps>
  simulationTiming() const override {
    if (_kind == DemoKind::Bistro)
      return runtime::SimulationTimingProps{};
    return {};
  }

  void fixedUpdate(AppContext &, runtime::SimulationStep,
                   const input::InputSnapshot &) override;
  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  void onActions(AppContext &, const input::InputSnapshot &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, rendering::RenderFrame &) override;
};
} // namespace playground::demo3d
