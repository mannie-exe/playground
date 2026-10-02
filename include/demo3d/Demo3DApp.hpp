#pragma once

#include <future>
#include <memory>
#include <optional>
#include <vector>

#include <app/IApp.hpp>
#include <assets/AssetCatalog.hpp>
#include <platform/sdl/UISession.hpp>
#include <runtime/Executor.hpp>
#include <scene/Animation.hpp>
#include <scene/Controllers.hpp>
#include <scene/Environment.hpp>
#include <scene/ModelImport.hpp>
#include <ui/content/SceneView.hpp>

namespace playground::demo3d {
void registerAssets(assets::AssetCatalog &catalog);

enum class DemoKind { Material, Bistro, Chess };

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
  input::InputSnapshot _navigation;
  scene::CameraProps camera() const;
  void updateView();
  float _exposure{1};
  bool _lighting{true};

  sdl::UISession _ui;
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

  input::InputClaims inputClaims() override { return _ui.inputClaims(); }

  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  void onActions(AppContext &, const input::InputSnapshot &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, rendering::RenderFrame &) override;
};
} // namespace playground::demo3d
