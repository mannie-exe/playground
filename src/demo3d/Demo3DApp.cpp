#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_scancode.h>

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

const assets::AssetId<assets::ModelAsset> flightId{"demo3d.flight-helmet"},
    helmetId{"demo3d.scifi-helmet"}, bistroId{"demoscene.bistro"},
    chessId{"demoscene.chess"};

scene::ModelImportProps
importProps(const assets::AssetId<assets::ModelAsset> &id) {
  const bool large = id == bistroId;
  scene::ModelImportProps props;
  props.allowMaterialFallback = true;
  props.maxDocumentBytes = (large ? 320ULL : 64ULL) * 1024 * 1024;
  props.maxResourceBytes = (large ? 320ULL : 128ULL) * 1024 * 1024;
  props.maxTotalResourceBytes =
      (large || id == chessId ? 1024ULL : 768ULL) * 1024 * 1024;
  return props;
}

scene::Bounds3 bounds(const scene::Scene3D &scene) {
  const float inf = std::numeric_limits<float>::infinity();
  scene::Bounds3 result{{inf, inf, inf}, {-inf, -inf, -inf}};
  for (const auto &draw : scene.snapshot()) {
    const auto b = draw.mesh->bounds();
    for (int corner = 0; corner < 8; ++corner) {
      auto p = math::transformPoint(draw.model,
                                    {corner & 1 ? b.maximum.x : b.minimum.x,
                                     corner & 2 ? b.maximum.y : b.minimum.y,
                                     corner & 4 ? b.maximum.z : b.minimum.z});
      result.minimum = {std::min(result.minimum.x, p.x),
                        std::min(result.minimum.y, p.y),
                        std::min(result.minimum.z, p.z)};
      result.maximum = {std::max(result.maximum.x, p.x),
                        std::max(result.maximum.y, p.y),
                        std::max(result.maximum.z, p.z)};
    }
  }
  if (!math::isFinite(result.minimum) || !math::isFinite(result.maximum))
    throw std::runtime_error("Demo scene contains no bounded geometry");
  return result;
}

scene::MeshHandle referenceSphere() {
  scene::MeshData data;
  constexpr int rings = 16, segments = 32;
  for (int y = 0; y <= rings; ++y) {
    const float v = float(y) / rings, phi = v * std::numbers::pi_v<float>;
    for (int x = 0; x <= segments; ++x) {
      const float u = float(x) / segments,
                  theta = (u - .5f) * 2 * std::numbers::pi_v<float>;
      const math::Vec3f p{std::sin(theta) * std::sin(phi), std::cos(phi),
                          std::cos(theta) * std::sin(phi)};
      data.vertices.push_back({.position = p, .normal = p, .uv = {u, v}});
    }
  }
  for (int y = 0; y < rings; ++y)
    for (int x = 0; x < segments; ++x) {
      const auto a = static_cast<std::uint32_t>(y * (segments + 1) + x),
                 b = a + segments + 1;
      data.indices.insert(data.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
    }
  return scene::makeMesh(std::move(data));
}
} // namespace

void registerAssets(assets::AssetCatalog &catalog) {
  for (const auto &[id, path] :
       std::vector<std::pair<assets::AssetId<assets::ModelAsset>, std::string>>{
           {propId, "demo3d/BoomBox.glb"},
           {flightId, "demo3d/FlightHelmet.glb"},
           {helmetId, "demo3d/SciFiHelmet.glb"},
           {chessId, "demoscene/chess/ABeautifulGame.glb"},
           {bistroId, "demoscene/bistro/Bistro.glb"}})
    catalog.add(id,
                assets::ModelAsset{assets::FileSource{path}, importProps(id)});
  catalog.add(smokeId, assets::BinaryAsset{
                           assets::FileSource{"demo3d/Smoke30Frames.png"}});
  catalog.add(environmentId, assets::BinaryAsset{assets::FileSource{
                                 "demo3d/studio_small_09_1k.hdr"}});
}

Demo3DApp::Demo3DApp(DemoKind kind) : _kind{kind} {
  input().addContext({.name = "demo3d"},
                     {{.action = "left", .code = SDL_SCANCODE_LEFT},
                      {.action = "right", .code = SDL_SCANCODE_RIGHT},
                      {.action = "up", .code = SDL_SCANCODE_UP},
                      {.action = "down", .code = SDL_SCANCODE_DOWN},
                      {.action = "forward", .code = SDL_SCANCODE_W},
                      {.action = "backward", .code = SDL_SCANCODE_S},
                      {.action = "strafe-left", .code = SDL_SCANCODE_A},
                      {.action = "strafe-right", .code = SDL_SCANCODE_D},
                      {.action = "rise", .code = SDL_SCANCODE_PAGEUP},
                      {.action = "fall", .code = SDL_SCANCODE_PAGEDOWN},
                      {.action = "exposure-up", .code = SDL_SCANCODE_E},
                      {.action = "exposure-down", .code = SDL_SCANCODE_Q},
                      {.action = "reset", .code = SDL_SCANCODE_R},
                      {.action = "pause", .code = SDL_SCANCODE_SPACE},
                      {.action = "light", .code = SDL_SCANCODE_L}});
}

Demo3DApp::~Demo3DApp() {
  if (_task)
    _task->cancel();
}

AppInfo Demo3DApp::staticInfo(DemoKind kind) {
  const auto id = kind == DemoKind::Bistro  ? AppId::Bistro
                  : kind == DemoKind::Chess ? AppId::Chess
                                            : AppId::Demo3D;
  AppInfo info{.id = id,
               .name = toString(id),
               .window = {.title = std::string{toString(id)}}};
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
      ui::TextProps{
          .value = "Preparing textures and environment...",
          .font = ctx.resources().font(app::fontAsset, {.style = {.size = 20}}),
          .textRole = ui::TextRole::Body}));
  auto promise = std::make_shared<std::promise<Resources>>();
  _pending = promise->get_future();
  auto catalog = ctx.resources().catalogHandle();
  _task = ctx.workers().submit(
      [catalog, promise, kind = _kind](std::stop_token stop) noexcept {
        try {
          Resources result;
          const auto ids = kind == DemoKind::Material
                               ? std::vector{propId, flightId, helmetId}
                           : kind == DemoKind::Bistro ? std::vector{bistroId}
                                                      : std::vector{chessId};
          for (const auto &id : ids) {
            try {
              result.models.push_back(sdl::prepareModel(*catalog, id, stop));
            } catch (const std::exception &error) {
              throw std::runtime_error(id.value + ": " + error.what());
            }
          }
          auto bytes = catalog->read(catalog->definition(environmentId).source,
                                     16 * 1024 * 1024);
          auto hdr = sdl::decodeHDR(bytes);
          result.environment = scene::prepareEnvironment(*hdr, {}, stop);
          if (kind == DemoKind::Material) {
            result.environmentReference =
                std::make_shared<const rendering::Texture>(
                    rendering::TextureRole::Color, hdr->levels());
            bytes = catalog->read(catalog->definition(smokeId).source,
                                  16 * 1024 * 1024);
            // Unpadded atlas: no whole-sheet mips, which would blend adjacent
            // frames.
            result.smoke = sdl::decodeTexture(bytes, "image/png",
                                              rendering::TextureRole::Color,
                                              rendering::MipPolicy::None);
          }
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
  if (_kind == DemoKind::Material) {
    for (std::size_t i = 0; i < _resources.models.size(); ++i) {
      scene::Scene3D sample;
      _resources.models[i]->instantiate(sample);
      const auto box = bounds(sample);
      const auto extent = box.maximum - box.minimum;
      const float scale = 2.f / std::max({extent.x, extent.y, extent.z, .001f});
      const auto center = (box.maximum + box.minimum) * .5f;
      _resources.models[i]->instantiate(
          *_scene, {.position = math::Vec3f{(float(i) - 1) * 2.7f, 1, 0} -
                                center * scale,
                    .scale = {scale, scale, scale}});
    }
    scene::MaterialProps reference;
    reference.doubleSided = true;
    reference.colorTexture.texture = _resources.environmentReference;
    _scene->create(
        {.transform = {.position = {-1, -1, 0}, .scale = {.6f, .6f, .6f}},
         .mesh = referenceSphere(),
         .material = reference});
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
    _smoke = _scene->create({.transform = {.position = {1, -1, 0}},
                             .mesh = scene::makeMesh(std::move(quad)),
                             .material = smoke});
    _camera.setProps({.target = {0, .5f, 0}, .pitch = .15f, .distance = 10});
  } else {
    _resources.models.front()->instantiate(*_scene);
    const auto box = bounds(*_scene);
    const auto center = (box.minimum + box.maximum) * .5f;
    const auto extent = box.maximum - box.minimum;
    const float radius = std::max({extent.x, extent.y, extent.z, 1.f});
    _initialCamera = {.position = center +
                                  math::Vec3f{0, radius * .35f, -radius * 1.1f},
                      .pitch = -.3f,
                      .unitsPerSecond = radius * .15f};
    if (_kind == DemoKind::Bistro)
      _initialCamera = {.position = {24.82285f, 3.16055f, 61.64814f},
                        .yaw = -2.81696f,
                        .pitch = -.05827f,
                        .unitsPerSecond = 5};
    _freeCamera.setProps(_initialCamera);
  }
  std::size_t warningCount{};
  for (const auto &model : _resources.models) {
    warningCount += model->warnings().size();
    for (const auto &warning : model->warnings())
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: %s",
                  std::string{info().name}.c_str(), warning.c_str());
  }
  auto root = std::make_unique<ui::VStack>(
      layout::StackProps{.childrenAlignment = layout::CrossAlignment::Stretch});
  root->append(std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{
          .value =
              (_kind == DemoKind::Material
                   ? std::string{"BoomBox / Flight Helmet / SciFi Helmet | "
                                 "Arrows: orbit | W/S: zoom | Space: smoke"}
                   : std::string{"WASD: fly | Arrows: look | PgUp/PgDn: "
                                 "rise/fall | R: reset"}) +
              " | Q/E: exposure | L: direct light | Esc: settings" +
              (warningCount ? " | Approximate materials (see log)" : ""),
          .font = ctx.resources().font(app::fontAsset, {.style = {.size = 16}}),
          .wrap = ui::TextWrap::AvailableInlineSize,
          .textRole = ui::TextRole::Caption}));
  ui::SceneViewProps props{
      .scene = _scene, .camera = camera(), .preferredSize = {900, 650}};
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

scene::CameraProps Demo3DApp::camera() const {
  return _kind == DemoKind::Material
             ? _camera.camera()
             : _freeCamera.camera({.nearPlane = .02f, .farPlane = 5000});
}

void Demo3DApp::updateView() {
  if (!_view)
    return;
  auto props = _view->props();
  props.camera = camera();
  props.exposure = _exposure;
  props.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
  _view->setProps(std::move(props));
}

void Demo3DApp::onActions(AppContext &, const input::InputSnapshot &actions) {
  _navigation = actions;
  if (actions["pause"].pressed)
    _playback.setPaused(!_playback.isPaused());
  if (actions["light"].pressed)
    _lighting = !_lighting;
  if (actions["exposure-up"].pressed)
    _exposure = std::min(16.f, _exposure * 1.25f);
  if (actions["exposure-down"].pressed)
    _exposure = std::max(.0625f, _exposure / 1.25f);
  if (_kind == DemoKind::Material) {
    _camera.update({.radians = {(float(actions["right"].pressed) -
                                 float(actions["left"].pressed)) *
                                    .12f,
                                (float(actions["down"].pressed) -
                                 float(actions["up"].pressed)) *
                                    .12f},
                    .zoom = (float(actions["backward"].pressed) -
                             float(actions["forward"].pressed)) *
                            .25f});
  } else if (actions["reset"].pressed) {
    _freeCamera.setProps(_initialCamera);
  }
  updateView();
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
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Scene preparation failed: %s",
                  e.what());
      _view = nullptr;
      _scene.reset();
      _resources = {};
      _ui.root().setContent(std::make_unique<ui::Text>(
          ctx.assets(),
          ui::TextProps{.value = std::string{"Scene preparation failed: "} +
                                 e.what(),
                        .font = ctx.resources().font(app::fontAsset),
                        .wrap = ui::TextWrap::AvailableInlineSize,
                        .textRole = ui::TextRole::Body,
                        .ink = ui::TextInk::Error}));
    }
    _task.reset();
  }
  if (_view && _kind != DemoKind::Material) {
    auto held = [&](const char *key) { return float(_navigation[key].held); };
    _freeCamera.advance(
        {.movement = {held("strafe-right") - held("strafe-left"),
                      held("rise") - held("fall"),
                      held("forward") - held("backward")},
         .radians = {(held("right") - held("left")) * dt,
                     (held("up") - held("down")) * dt}},
        dt);
    updateView();
  }
  if (_view && _kind == DemoKind::Material) {
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
        scene::billboard({1, -1, 0}, _camera.camera(), {1.5f, 1.5f, 1.5f});
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
  _navigation = {};
  _ui.clear();
  _view = nullptr;
  _scene.reset();
  _resources = {};
}
} // namespace playground::demo3d
