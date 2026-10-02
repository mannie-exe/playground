#pragma once

#include <future>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <app/IApp.hpp>
#include <assets/AssetCatalog.hpp>
#include <platform/sdl/UISession.hpp>
#include <runtime/Benchmark.hpp>
#include <runtime/Executor.hpp>
#include <scene/Animation.hpp>
#include <scene/Controllers.hpp>
#include <scene/Environment.hpp>
#include <scene/ModelImport.hpp>
#include <ui/content/SceneView.hpp>
#include <ui/content/Text.hpp>

namespace playground::demo3d {
void registerAssets(assets::AssetCatalog &catalog);

enum class DemoKind { Material, Bistro, Chess, Benchmark };

class Demo3DApp final : public IApp {
  struct Resources {
    std::vector<scene::ModelHandle> models;
    scene::PreparedEnvironment environment;
    rendering::TextureHandle smoke, environmentReference;
  };

  DemoKind _kind;
  Resources _resources;
  std::shared_ptr<scene::Scene3D> _scene;
  scene::ObjectId _smoke;
  scene::Playback _playback;
  scene::OrbitController _camera;
  scene::FreeCameraController _freeCamera;
  scene::FreeCameraProps _initialCamera;
  scene::CameraDirector _director{{}, 0};
  scene::CameraId _interactiveCamera{};
  ui::Connection _mouseLock;
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
  static const scene::CameraPath &benchmarkPath();
  input::InputSnapshot _navigation;
  scene::CameraProps camera() const;
  void updateView();
  float _exposure{1};
  bool _lighting{true};

  sdl::UISession _ui{sdl::UISessionTiming::Monotonic};
  ui::SceneView *_view{};

  std::future<Resources> _pending;
  std::optional<runtime::TaskTicket> _task;

  void createView(AppContext &ctx, Resources resources);
  void synchronize(AppContext &ctx);

public:
  explicit Demo3DApp(DemoKind kind = DemoKind::Material);
  ~Demo3DApp() override;
  static AppInfo staticInfo(DemoKind kind = DemoKind::Material);

  AppInfo info() const override { return staticInfo(_kind); }

  void configureLaunch(const AppLaunchProps &) override;

  void onActivityInterrupted(AppContext &,
                             AppInterruption reason) noexcept override {
    _mouseLock.disconnect();
    _navigation = {};
    if (_kind == DemoKind::Benchmark)
      _benchmark.invalidate(reason == AppInterruption::Focus
                                ? runtime::BenchmarkInterruption::Focus
                                : runtime::BenchmarkInterruption::Settings);
  }

  runtime::ActivityProps activityProps() const override {
    return {true,
            (_kind == DemoKind::Material && !_playback.isPaused()) ||
                (_kind == DemoKind::Benchmark && _benchmark.traversing())};
  }

  runtime::ActivityDemand activityDemand() override {
    return _ui.activityDemand();
  }

  input::InputClaims inputClaims() override { return _ui.inputClaims(); }

  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  void onActions(AppContext &, const input::InputSnapshot &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, rendering::RenderFrame &) override;
};
} // namespace playground::demo3d
