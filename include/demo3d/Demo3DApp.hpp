#pragma once

#include <future>
#include <memory>
#include <optional>

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

class Demo3DApp final : public IApp {
  struct Resources {
    scene::ModelHandle model;
    scene::PreparedEnvironment environment;
    rendering::TextureHandle smoke;
  };

  Resources _resources;
  std::shared_ptr<scene::Scene3D> _scene;
  scene::ObjectId _smoke;
  scene::Playback _playback;
  scene::OrbitController _camera;
  float _exposure{1};
  bool _lighting{true};

  sdl::UISession _ui;
  ui::SceneView *_view{};

  std::future<Resources> _pending;
  std::optional<runtime::TaskTicket> _task;

  void createView(AppContext &ctx, Resources resources);
  void synchronize(AppContext &ctx);

public:
  Demo3DApp();
  ~Demo3DApp() override;
  static AppInfo staticInfo();

  AppInfo info() const override { return staticInfo(); }

  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  void onActions(AppContext &, const input::InputSnapshot &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, rendering::RenderFrame &) override;
};
} // namespace playground::demo3d
