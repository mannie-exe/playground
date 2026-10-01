#include <algorithm>
#include <chrono>
#include <memory>

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_log.h>

#include <app/Assets.hpp>
#include <demo3d/Demo3DApp.hpp>
#include <platform/sdl/ModelPreparation.hpp>
#include <platform/sdl/TextureDecode.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/content/Text.hpp>

namespace playground::demo3d {
namespace {
const assets::AssetId<assets::ModelAsset> propId{"demo3d.prop"};
const assets::AssetId<assets::BinaryAsset> smokeId{"demo3d.smoke"},
    environmentId{"demo3d.environment"};

scene::ModelImportProps importProps() {
  scene::ModelImportProps props;
  props.maxTotalResourceBytes = 512 * 1024 * 1024;
  return props;
}
} // namespace

void registerAssets(assets::AssetCatalog &catalog) {
  catalog.add(propId,
              assets::ModelAsset{assets::FileSource{"demo3d/BoomBox.glb"},
                                 importProps()});
  catalog.add(smokeId, assets::BinaryAsset{
                           assets::FileSource{"demo3d/Smoke30Frames.png"}});
  catalog.add(environmentId, assets::BinaryAsset{assets::FileSource{
                                 "demo3d/studio_small_09_1k.hdr"}});
}

Demo3DApp::Demo3DApp() {
  input().addContext({.name = "demo3d"},
                     {{.action = "left", .code = SDL_SCANCODE_LEFT},
                      {.action = "right", .code = SDL_SCANCODE_RIGHT},
                      {.action = "up", .code = SDL_SCANCODE_UP},
                      {.action = "down", .code = SDL_SCANCODE_DOWN},
                      {.action = "zoom-in", .code = SDL_SCANCODE_W},
                      {.action = "zoom-out", .code = SDL_SCANCODE_S},
                      {.action = "exposure-up", .code = SDL_SCANCODE_E},
                      {.action = "exposure-down", .code = SDL_SCANCODE_Q},
                      {.action = "pause", .code = SDL_SCANCODE_SPACE},
                      {.action = "light", .code = SDL_SCANCODE_L}});
}

Demo3DApp::~Demo3DApp() {
  if (_task)
    _task->cancel();
}

AppInfo Demo3DApp::staticInfo() {
  AppInfo info{
      .id = AppId::Demo3D, .name = "Demo 3D", .window = {.title = "Demo 3D"}};
  info.view.preferredWindowSize = math::Vec2i{1000, 760};
  info.view.minimumSize = {400, 300};
  info.presentation.renderer = {rendering::RendererChoice::SDLGPU,
                                rendering::GPUDriver::Vulkan, false};
  info.rendererRequirements = {
      .scene3D = true, .linearComposition = true, .metallicRoughness = true};
  return info;
}

void Demo3DApp::onEnter(AppContext &ctx) {
  _ui.root().setContent(std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{.value = "Preparing textures and environment...",
                    .font = ctx.resources().font(app::fontAsset,
                                                 {.style = {.size = 20}})}));
  auto promise = std::make_shared<std::promise<Resources>>();
  _pending = promise->get_future();
  auto catalog = ctx.resources().catalogHandle();
  _task = ctx.workers().submit(
      [catalog, promise](std::stop_token stop) noexcept {
        try {
          Resources result;
          result.model = sdl::prepareModel(*catalog, propId, stop);
          auto bytes = catalog->read(catalog->definition(environmentId).source,
                                     16 * 1024 * 1024);
          auto hdr = sdl::decodeHDR(bytes);
          result.environment = scene::prepareEnvironment(*hdr, {}, stop);
          bytes = catalog->read(catalog->definition(smokeId).source,
                                16 * 1024 * 1024);
          // Unpadded atlas: no whole-sheet mips, which would blend adjacent
          // frames.
          result.smoke = sdl::decodeTexture(bytes, "image/png",
                                            rendering::TextureRole::Color,
                                            rendering::MipPolicy::None);
          if (stop.stop_requested())
            throw std::runtime_error("Demo 3D preparation canceled");
          promise->set_value(std::move(result));
        } catch (...) {
          promise->set_exception(std::current_exception());
        }
      },
      512 * 1024 * 1024);
  if (!_task)
    throw std::runtime_error("Demo 3D worker admission refused");
  synchronize(ctx);
}

void Demo3DApp::synchronize(AppContext &ctx) { _ui.synchronize(ctx); }

void Demo3DApp::createView(AppContext &ctx, Resources resources) {
  _resources = std::move(resources);
  _scene = std::make_shared<scene::Scene3D>();
  // The sample is authored in meters and only a few centimeters across.
  _resources.model->instantiate(*_scene, {.scale = {100, 100, 100}});
  scene::MeshData quad{{{{-.5f, -.5f, 0}, {0, 0, -1}, {0, 1}},
                        {{.5f, -.5f, 0}, {0, 0, -1}, {1, 1}},
                        {{.5f, .5f, 0}, {0, 0, -1}, {1, 0}},
                        {{-.5f, .5f, 0}, {0, 0, -1}, {0, 0}}},
                       {0, 2, 1, 0, 3, 2}};
  scene::MaterialProps smoke;
  smoke.alpha = scene::MaterialProps::Alpha::Blend;
  smoke.colorTexture.texture = _resources.smoke;
  smoke.colorTexture.sampler.mip = rendering::MipFilter::None;
  smoke.colorTexture.sampler.addressU = smoke.colorTexture.sampler.addressV =
      rendering::TextureAddress::Clamp;
  _smoke = _scene->create({.transform = {.position = {1.7f, 0, 0}},
                           .mesh = scene::makeMesh(std::move(quad)),
                           .material = smoke});
  _camera.setProps({.distance = 5});
  auto root = std::make_unique<ui::VStack>(layout::StackProps{.gap = 8});
  root->append(std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{
          .value = "Arrows: orbit | W/S: zoom | Q/E: exposure | L: "
                   "direct light | Space: smoke pause | Esc: settings",
          .font = ctx.resources().font(app::fontAsset, {.style = {.size = 16}}),
          .wrap = ui::TextWrap::AvailableInlineSize}));
  ui::SceneViewProps props{
      .scene = _scene, .camera = _camera.camera(), .preferredSize = {900, 650}};
  props.lighting.diffuseEnvironment = _resources.environment.diffuse;
  props.lighting.specularEnvironment = _resources.environment.specular;
  props.lighting.brdf = _resources.environment.brdf;
  props.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
  props.exposure = _exposure;
  props.toneMap = true;
  auto view = std::make_unique<ui::SceneView>(props);
  _view = view.get();
  root->append(std::move(view), {.grow = 1});
  _ui.root().setContent(std::move(root));
}

void Demo3DApp::onActions(AppContext &ctx,
                          const input::InputSnapshot &actions) {
  if (actions["pause"].pressed)
    _playback.setPaused(!_playback.isPaused());
  const bool viewChanged =
      actions["light"].pressed || actions["exposure-up"].pressed ||
      actions["exposure-down"].pressed || actions["left"].pressed ||
      actions["right"].pressed || actions["up"].pressed ||
      actions["down"].pressed || actions["zoom-in"].pressed ||
      actions["zoom-out"].pressed;
  if (!viewChanged)
    return;
  if (actions["light"].pressed)
    _lighting = !_lighting;
  if (actions["exposure-up"].pressed)
    _exposure = std::min(16.f, _exposure * 1.25f);
  if (actions["exposure-down"].pressed)
    _exposure = std::max(.0625f, _exposure / 1.25f);
  _camera.update({.radians = {(float(actions["right"].pressed) -
                               float(actions["left"].pressed)) *
                                  .12f,
                              (float(actions["down"].pressed) -
                               float(actions["up"].pressed)) *
                                  .12f},
                  .zoom = (float(actions["zoom-out"].pressed) -
                           float(actions["zoom-in"].pressed)) *
                          .25f});
  if (_view) {
    auto view = _view->props();
    view.camera = _camera.camera();
    view.exposure = _exposure;
    view.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
    _view->setProps(std::move(view));
  }
}

EventResult Demo3DApp::handleEvent(AppContext &ctx, const SDL_Event &event) {
  synchronize(ctx);
  return _ui.handleEvent(event);
}

void Demo3DApp::update(AppContext &ctx, float dt) {
  if (_pending.valid() &&
      _pending.wait_for(std::chrono::seconds{0}) == std::future_status::ready) {
    try {
      createView(ctx, _pending.get());
    } catch (const std::exception &e) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Material preparation failed: %s", e.what());
      _view = nullptr;
      _scene.reset();
      _resources = {};
      _ui.root().setContent(std::make_unique<ui::Text>(
          ctx.assets(),
          ui::TextProps{.value = std::string{"Material preparation failed: "} +
                                 e.what(),
                        .font = ctx.resources().font(app::fontAsset),
                        .wrap = ui::TextWrap::AvailableInlineSize}));
    }
    _task.reset();
  }
  if (_view) {
    _playback.advance(dt);
    auto props = _scene->props(_smoke);
    const auto size = _resources.smoke->levels().front().size;
    props.material.colorTexture.transform = scene::flipbookFrame(
        {.grid = {6, 5},
         .frames = 30,
         .framesPerSecond = 15,
         .inset = {.5f / size.x, .5f / size.y},
         .pixels = scene::FlipbookProps::PixelGrid{size, {256, 256}}},
        _playback);
    props.transform =
        scene::billboard({1.7f, 0, 0}, _camera.camera(), {1.5f, 1.5f, 1.5f});
    const auto &previous = _scene->props(_smoke);
    if (previous.transform != props.transform ||
        previous.material.colorTexture.transform !=
            props.material.colorTexture.transform)
      _scene->setProps(_smoke, std::move(props));
  }
  _ui.update(dt);
}

void Demo3DApp::render(AppContext &ctx, rendering::RenderFrame &frame) {
  synchronize(ctx);
  _ui.render(frame);
}

void Demo3DApp::onExit(AppContext &) {
  if (_task)
    _task->cancel();
  _task.reset();
  _pending = {};
  _ui.clear();
  _view = nullptr;
  _scene.reset();
  _resources = {};
}
} // namespace playground::demo3d
