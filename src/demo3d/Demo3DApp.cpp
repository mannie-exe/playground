#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
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
#include <platform/sdl/WindowServices.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/content/Text.hpp>

namespace playground::demo3d {
namespace {
double nowSeconds() {
  return std::chrono::duration<double>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

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
  const auto id = kind == DemoKind::Benchmark ? AppId::BistroBenchmark
                  : kind == DemoKind::Bistro  ? AppId::Bistro
                  : kind == DemoKind::Chess   ? AppId::Chess
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
  _loadStarted = nowSeconds();
  _benchmarkSubmitted = ctx.renderRuntimeState().submitted;
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
          const auto ids =
              kind == DemoKind::Material
                  ? std::vector{propId, flightId, helmetId}
              : (kind == DemoKind::Bistro || kind == DemoKind::Benchmark)
                  ? std::vector{bistroId}
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
    if (_kind == DemoKind::Bistro || _kind == DemoKind::Benchmark)
      _initialCamera = {.position = {24.82285f, 3.16055f, 61.64814f},
                        .yaw = -2.81696f,
                        .pitch = -.05827f,
                        .unitsPerSecond = 5};
    _freeCamera.setProps(_initialCamera);
  }
  _director.setFallback(camera());
  _interactiveCamera = _director.add({.camera = camera()});
  if (_kind == DemoKind::Benchmark)
    _pathCamera =
        _director.add({.camera = benchmarkPath().sample(0), .priority = 10});
  std::size_t warningCount{};
  for (const auto &model : _resources.models) {
    warningCount += model->warnings().size();
    for (const auto &warning : model->warnings())
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s: %s",
                  std::string{info().name}.c_str(), warning.c_str());
  }
  auto root = std::make_unique<ui::VStack>(
      layout::StackProps{.childrenAlignment = layout::CrossAlignment::Stretch});
  auto caption = std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{
          .value =
              (_kind == DemoKind::Material
                   ? std::string{"BoomBox / Flight Helmet / SciFi Helmet | "
                                 "Arrows: orbit | W/S: zoom | Space: smoke"}
                   : std::string{"WASD: fly | Arrows: look | PgUp/PgDn: "
                                 "rise/fall | R: reset"}) +
              " | RMB: mouse look | Esc: unlock/settings | Q/E: exposure | L: "
              "light" +
              (warningCount ? " | Approximate materials (see log)" : ""),
          .font = ctx.resources().font(app::fontAsset, {.style = {.size = 16}}),
          .wrap = ui::TextWrap::AvailableInlineSize,
          .textRole = ui::TextRole::Caption});
  _caption = caption.get();
  if (_kind == DemoKind::Benchmark) {
    _caption->setBoxProps({.height = layout::SizeRule::fixed(64)});
    _caption->applyPatch({.value = Patch<std::string>::set(
                              "Bistro benchmark: preparing resources")});
  }
  root->append(std::move(caption));
  ui::SceneViewProps props{.scene = _scene,
                           .camera = _director.camera(),
                           .preferredSize = {900, 650}};
  props.lighting.diffuseEnvironment = _resources.environment.diffuse;
  props.lighting.specularEnvironment = _resources.environment.specular;
  props.lighting.brdf = _resources.environment.brdf;
  props.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
  props.exposure = _exposure;
  props.toneMap = true;
  auto view = std::make_unique<ui::SceneView>(props);
  _view = view.get();
  if (_kind == DemoKind::Benchmark)
    restartBenchmark(ctx);
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
  _director.update(_interactiveCamera, {.camera = camera()});
  props.camera = _director.camera();
  props.exposure = _exposure;
  props.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
  _view->setProps(std::move(props));
}

void Demo3DApp::onActions(AppContext &ctx,
                          const input::InputSnapshot &actions) {
  if (_kind == DemoKind::Benchmark) {
    if (actions["reset"].pressed && _view)
      restartBenchmark(ctx);
    return;
  }
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
  if (_kind == DemoKind::Benchmark) {
    if (_benchmark.traversing() &&
        (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
         event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
         (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)))
      _benchmark.invalidate();
    return _ui.handleEvent(event);
  }
  auto &window = ctx.windowServices();
  if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE &&
      window.relativeMouseActive()) {
    _mouseLock.disconnect();
    window.releaseRelativeMouse();
    input().cancelAll();
    _navigation = {};
    return EventResult::Consumed;
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION && window.relativeMouseActive()) {
    const math::Vec2f delta{event.motion.xrel * .003f,
                            -event.motion.yrel * .003f};
    if (_kind == DemoKind::Material)
      _camera.update({.radians = {-delta.x, delta.y}});
    else
      _freeCamera.advance({.radians = delta}, 0);
    updateView();
    return EventResult::Consumed;
  }
  const auto result = _ui.handleEvent(event);
  if (result != EventResult::Ignored)
    return result;
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
      event.button.button == SDL_BUTTON_RIGHT && _view && _view->viewport() &&
      !_ui.inputClaims().pointer &&
      _view->viewport()->normalizedPosition(
          _ui.mapping().toLogical({event.button.x, event.button.y}))) {
    if (window.relativeMouseActive())
      _mouseLock.disconnect();
    else {
      try {
        _mouseLock = window.lockRelativeMouse(ctx.activationToken());
      } catch (const std::exception &error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
      }
    }
    return EventResult::Consumed;
  }
  return result;
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
  if (_view && _kind == DemoKind::Benchmark)
    updateBenchmark(ctx);
  if (_view && _kind != DemoKind::Material && _kind != DemoKind::Benchmark) {
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

void Demo3DApp::configureLaunch(const AppLaunchProps &props) {
  if (_kind != DemoKind::Benchmark) {
    IApp::configureLaunch(props);
    return;
  }
  _benchmark = runtime::BenchmarkRun{props.benchmarkSeconds.value_or(15)};
}

const scene::CameraPath &Demo3DApp::benchmarkPath() {
  static const scene::CameraPath path{
      15,
      {{0,
        {.eye = {24.82285f, 3.16055f, 61.64814f},
         .target = {24.50443f, 3.10232f, 60.70198f},
         .nearPlane = .02f,
         .farPlane = 5000}},
       {5,
        {.eye = {22, 3.3f, 53},
         .target = {20, 3.1f, 48},
         .nearPlane = .02f,
         .farPlane = 5000}},
       {10,
        {.eye = {20, 3.4f, 45},
         .target = {24, 3.1f, 57},
         .nearPlane = .02f,
         .farPlane = 5000}}}};
  return path;
}

void Demo3DApp::restartBenchmark(AppContext &ctx) {
  _benchmark = runtime::BenchmarkRun{_benchmark.duration()};
  _benchmarkSubmitted = ctx.renderRuntimeState().submitted;
  _seenCPU = ctx.renderRuntimeState().cpuSamples;
  _seenGPU.clear();
  _benchmarkReported = false;
  _caption->setBoxProps({.height = layout::SizeRule::fixed(64)});
  _measuredSubmitted = ctx.renderRuntimeState().submitted;
  _finalSubmitted = 0;
  _benchmarkWork = ctx.renderRuntimeState().sceneWork;
  _director.update(_pathCamera,
                   {.camera = benchmarkPath().sample(0), .priority = 10});
  auto props = _view->props();
  props.camera = _director.camera();
  _view->setProps(std::move(props));
  _caption->applyPatch(
      {.value = Patch<std::string>::set("Bistro benchmark: preparing resources "
                                        "| Esc: settings | R: restart")});
}

void Demo3DApp::updateBenchmark(AppContext &ctx) {
  using runtime::BenchmarkPhase;
  const auto now = nowSeconds();
  const auto state = ctx.renderRuntimeState();
  const auto telemetry = ctx.renderTelemetry();
  const auto available = std::min<std::uint64_t>(state.cpuSamples - _seenCPU,
                                                 telemetry.cpu.size());
  if (_benchmark.phase() == BenchmarkPhase::Measuring)
    _benchmark.omittedCPU += state.cpuSamples - _seenCPU - available;
  for (std::size_t i = telemetry.cpu.size() - available;
       i < telemetry.cpu.size(); ++i)
    _benchmark.record(telemetry.cpu[i]);
  _seenCPU = state.cpuSamples;
  decltype(_seenGPU) seen;
  for (const auto &sample : telemetry.gpu) {
    auto key = std::tuple{sample.domain.value, sample.collectionGeneration,
                          sample.sequence};
    seen.insert(key);
    if (!_seenGPU.contains(key) && sample.domain == _benchmarkDomain)
      _benchmark.record(sample);
  }
  _seenGPU = std::move(seen);
  auto before = _benchmark.phase();
  if (before == BenchmarkPhase::Loading &&
      state.submitted > _benchmarkSubmitted) {
    if (!_loadedSeconds)
      _loadedSeconds = now - _loadStarted;
    _benchmark.ready(now);
    _benchmarkGraphics = ctx.graphicsState().requested;
    _benchmarkPixels = ctx.windowState().drawableSize;
    _benchmarkDomain =
        ctx.rendererState().selected.backend == rendering::RendererKind::SDLGPU
            ? state.gpu.domain
            : rendering::ResourceDomainId{};
  }
  if (_benchmark.traversing() &&
      (ctx.graphicsState().requested != _benchmarkGraphics ||
       ctx.windowState().drawableSize != _benchmarkPixels ||
       state.gpu.domain != _benchmarkDomain))
    _benchmark.invalidate();
  if (_benchmark.phase() == BenchmarkPhase::Measuring &&
      ctx.graphicsState().sceneScale != _benchmarkScale)
    _benchmark.invalidate();
  _benchmark.advance(now, state.admitted,
                     state.outstandingFrames || state.gpu.pending);
  if (before != BenchmarkPhase::Measuring &&
      _benchmark.phase() == BenchmarkPhase::Measuring) {
    _benchmarkWork = state.sceneWork;
    _benchmarkScale = ctx.graphicsState().sceneScale;
    _measuredSubmitted = state.submitted;
    _finalSubmitted = 0;
    _queryDrops = state.gpu.queryDrops;
    _bufferDiscards = state.gpu.bufferDiscards;
    _lastReport = now;
  }
  if (before == BenchmarkPhase::Measuring &&
      _benchmark.phase() == BenchmarkPhase::Draining)
    _finalSubmitted = state.submitted;
  if (_benchmark.traversing()) {
    _director.update(_pathCamera,
                     {.camera = benchmarkPath().sample(_benchmark.elapsed(now)),
                      .priority = 10});
    auto props = _view->props();
    props.camera = _director.camera();
    _view->setProps(std::move(props));
  }
  if (before != _benchmark.phase()) {
    _caption->applyPatch(
        {.value = Patch<std::string>::set(std::format(
             "Bistro benchmark: {} | warm-up 15s | run {}s (0=infinite) | R: "
             "restart | Esc: settings",
             runtime::toString(_benchmark.phase()), _benchmark.duration()))});
    SDL_Log("Benchmark phase: %s",
            std::string{runtime::toString(_benchmark.phase())}.c_str());
  }
  if (_benchmark.finished() && !_benchmarkReported) {
    reportBenchmark(ctx, true);
    _benchmarkReported = true;
  } else if (!_benchmark.duration() &&
             _benchmark.phase() == BenchmarkPhase::Measuring &&
             now - _lastReport >= 5) {
    reportBenchmark(ctx, false);
    _lastReport = now;
  }
}

void Demo3DApp::reportBenchmark(AppContext &ctx, bool final) {
  const auto state = ctx.renderRuntimeState();
  const auto &cpu = _benchmark.cpu[std::size_t(rendering::CPUPhase::Render)];
  const auto &gpu = _benchmark.gpuScene;
  const auto pixels =
      _view->viewport() ? _view->viewport()->pixelSize : math::Vec2i{};
  auto message = std::format(
      "BenchmarkJSON "
      "{{\"scene\":\"demoscene.bistro@1\",\"path\":\"bistro-street-v1\","
      "\"phase\":\"{}\",\"final\":{},\"requested_seconds\":{},\"measured_"
      "seconds\":{:.6f},\"load_seconds\":{:.6f},\"warmup_seconds\":15,"
      "\"domain\":{},\"width\":{},\"height\":{},\"vsync\":{},\"scene_scale\":{}"
      ",\"cpu_render_samples\":{},\"cpu_render_p50_ms\":{:.6f},\"cpu_render_"
      "p95_ms\":{:.6f},\"cpu_render_p99_ms\":{:.6f},\"cpu_render_max_ms\":{:."
      "6f},\"gpu_scene_samples\":{},\"gpu_scene_mean_ms\":{:.6f},\"uploads\":{}"
      ",\"upload_bytes\":{},\"cpu_omitted\":{},\"drain_timed_out\":{}}}",
      runtime::toString(_benchmark.phase()), final, _benchmark.duration(),
      _benchmark.measuredSeconds(), _loadedSeconds, _benchmarkDomain.value,
      pixels.x, pixels.y, _benchmarkGraphics.presentation.vsync,
      ctx.graphicsState().sceneScale, cpu.count, cpu.percentile(.5),
      cpu.percentile(.95), cpu.percentile(.99), cpu.maximum, gpu.count,
      gpu.count ? gpu.total / gpu.count : 0,
      state.sceneWork.uploads - _benchmarkWork.uploads,
      state.sceneWork.uploadBytes - _benchmarkWork.uploadBytes,
      _benchmark.omittedCPU, _benchmark.drainTimedOut);
  message.pop_back();
#ifdef NDEBUG
  constexpr auto build = "Release";
#else
  constexpr auto build = "Debug";
#endif
  message += std::format(
      ",\"build\":\"{}\",\"renderer\":\"sdl-gpu/vulkan\",\"platform\":\"{}\","
      "\"content_sha256\":"
      "\"fa23f764061fa5c1feadfbf38ca07c87f56d610ebf89cf4e1b816399de293c83\","
      "\"submitted_frames\":{},\"gpu_query_drops\":{},\"gpu_buffer_discards\":{"
      "},"
      "\"managed_cpu_peak_bytes\":{},\"managed_gpu_peak_bytes\":{},\"cpu_"
      "phases\":{{",
      build, SDL_GetPlatform(),
      (_finalSubmitted ? _finalSubmitted : state.submitted) -
          _measuredSubmitted,
      state.gpu.queryDrops >= _queryDrops ? state.gpu.queryDrops - _queryDrops
                                          : 0,
      state.gpu.bufferDiscards >= _bufferDiscards
          ? state.gpu.bufferDiscards - _bufferDiscards
          : 0,
      state.resources.memory[0].peak, state.resources.memory[1].peak);
  constexpr std::array names{"poll", "update", "render", "present", "total"};
  for (std::size_t i = 0; i < names.size(); ++i) {
    const auto &stats = _benchmark.cpu[i];
    message += std::format("{}\"{}\":{{\"samples\":{},\"p50_ms\":{},\"p95_ms\":"
                           "{},\"p99_ms\":{},\"max_ms\":{}}}",
                           i ? "," : "", names[i], stats.count,
                           stats.percentile(.5), stats.percentile(.95),
                           stats.percentile(.99), stats.maximum);
  }
  message += "}}";
  SDL_Log("%s", message.c_str());
  if (final) {
    _caption->setBoxProps({});
    _caption->applyPatch(
        {.value = Patch<std::string>::set(std::format(
             "Bistro benchmark: {} | CPU render p50 {:.2f}ms / p95 {:.2f}ms | "
             "GPU scene {:.2f}ms ({} samples) | uploads {} | R: restart",
             runtime::toString(_benchmark.phase()), cpu.percentile(.5),
             cpu.percentile(.95), gpu.count ? gpu.total / gpu.count : 0,
             gpu.count, state.sceneWork.uploads - _benchmarkWork.uploads))});
  }
}

void Demo3DApp::render(AppContext &ctx, rendering::RenderFrame &frame) {
  synchronize(ctx);
  _ui.render(frame);
}

void Demo3DApp::onExit(AppContext &ctx) {
  if (_kind == DemoKind::Benchmark && _view && !_benchmarkReported) {
    _benchmark.invalidate();
    reportBenchmark(ctx, true);
  }
  _mouseLock.disconnect();
  if (_pathCamera)
    _director.remove(std::exchange(_pathCamera, 0));
  if (_interactiveCamera)
    _director.remove(std::exchange(_interactiveCamera, 0));
  if (_task)
    _task->cancel();
  _task.reset();
  _pending = {};
  _navigation = {};
  _ui.clear();
  _view = nullptr;
  _caption = nullptr;
  _scene.reset();
  _resources = {};
}
} // namespace playground::demo3d
