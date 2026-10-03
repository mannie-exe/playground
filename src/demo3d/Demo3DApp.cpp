#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_scancode.h>

#include <app/Assets.hpp>
#include <demo3d/Demo3DApp.hpp>
#include <demo3d/Views.hpp>
#include <platform/sdl/AssetPreparation.hpp>
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

scene::MeshHandle
referenceSphere(const std::shared_ptr<runtime::ResourceLedger> &ledger) {
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
  return scene::makeMesh(std::move(data), ledger);
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

Demo3DApp::Demo3DApp(DemoKind kind,
                     std::shared_ptr<runtime::ResourceLedger> ledger)
    : _ledger{std::move(ledger)}, _kind{kind},
      _world{{std::uint64_t(kind) + 1}, _ledger},
      _space{_world.snapshot().id(), 1}, _scenery{_world.snapshot().id(), 1},
      _subject{_world.snapshot().id(), 2},
      _camera{{.target = {_space, {}}, .epoch = _world.snapshot().epoch()}},
      _freeCamera{
          {.position = {_space, {}}, .epoch = _world.snapshot().epoch()}},
      _director{_camera.camera(), 0},
      _controls{
          input(),
          {.actions = {std::vector<std::string>{"forward", "backward",
                                                "strafe-left", "strafe-right",
                                                "rise", "fall", "pad-move"},
                       std::vector<std::string>{"left", "right", "up", "down",
                                                "pointer-look", "pad-look"},
                       std::vector<std::string>{"wheel-zoom"},
                       {}}}} {
  const std::array<world::WorldMutation, 3> mutations{
      world::CreateSpace{{_space, {}}},
      world::SpawnEntity{_scenery,
                         {.pose = {{_space, {}}, {}}, .velocity = {_space}}},
      world::SpawnEntity{_subject,
                         {.pose = {{_space, {}}, {}}, .velocity = {_space}}}};
  _world.apply(mutations, _world.snapshot().version(), 0);
  if (_kind == DemoKind::Benchmark)
    _tour.emplace(benchmarkPath());
  input().addContext({.name = "demo3d"},
                     {{.action = "pointer-look",
                       .kind = input::ActionKind::Delta,
                       .control = input::ControlKind::PointerMotion,
                       .contribution = {1, 1}},
                      {.action = "wheel-zoom",
                       .kind = input::ActionKind::Delta,
                       .control = input::ControlKind::Wheel,
                       .contribution = {1, 1}},
                      {.action = "left", .code = SDL_SCANCODE_LEFT},
                      {.action = "right", .code = SDL_SCANCODE_RIGHT},
                      {.action = "up", .code = SDL_SCANCODE_UP},
                      {.action = "down", .code = SDL_SCANCODE_DOWN},
                      {.action = "forward", .code = SDL_SCANCODE_W},
                      {.action = "backward", .code = SDL_SCANCODE_S},
                      {.action = "strafe-left", .code = SDL_SCANCODE_A},
                      {.action = "strafe-right", .code = SDL_SCANCODE_D},
                      {.action = "rise", .code = SDL_SCANCODE_SPACE},
                      {.action = "fall", .code = SDL_SCANCODE_LSHIFT},
                      {.action = "exposure-up", .code = SDL_SCANCODE_E},
                      {.action = "exposure-down", .code = SDL_SCANCODE_Q},
                      {.action = "reset", .code = SDL_SCANCODE_R},
                      {.action = "follow", .code = SDL_SCANCODE_F},
                      {.action = "free-look", .code = SDL_SCANCODE_LALT},
                      {.action = "aim", .code = SDL_SCANCODE_LSHIFT},
                      {.action = "recenter", .code = SDL_SCANCODE_C},
                      {.action = "pause", .code = SDL_SCANCODE_P},
                      {.action = "light", .code = SDL_SCANCODE_L}});
}

Demo3DApp::~Demo3DApp() = default;

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
  applyControls(ctx);
  if (_kind == DemoKind::Bistro)
    ctx.setSimulationPaused(true);
  _loadStarted = nowSeconds();
  _benchmarkSubmitted = ctx.renderRuntimeState().submitted;
  _ui.root().setContent(std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{
          .value = "Preparing textures and environment...",
          .font = ctx.resources().font(app::fontAsset, {.style = {.size = 20}}),
          .textRole = ui::TextRole::Body}));
  _preparationProps.models = _kind == DemoKind::Material
                                 ? std::vector{propId, flightId, helmetId}
                                 : std::vector{(_kind == DemoKind::Bistro ||
                                                _kind == DemoKind::Benchmark)
                                                   ? bistroId
                                                   : chessId};
  _preparationProps.environment = sdl::EnvironmentRequest{environmentId};
  if (_kind == DemoKind::Material)
    _preparationProps.textures = {
        {smokeId, rendering::TextureRole::Color, rendering::MipPolicy::None}};
  _loadRequested = true;
  ctx.requestUpdate();
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
      const auto box = scene::drawBounds(sample.snapshot());
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
         .mesh = referenceSphere(_ledger),
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
                             .mesh = scene::makeMesh(std::move(quad), _ledger),
                             .material = smoke});
    _camera.setProps({.target = {_space, {0, .5, 0}},
                      .epoch = _world.snapshot().epoch(),
                      .pitch = .15,
                      .distance = 10});
  } else {
    _resources.models.front()->instantiate(*_scene);
    const auto box = scene::drawBounds(_scene->snapshot());
    const auto extent = box.maximum - box.minimum;
    const float radius = std::max({extent.x, extent.y, extent.z, 1.f});
    const auto eye = scene::boundsCamera(box, {}, 1.f).eye;
    _initialCamera = {.position = {_space, {eye.x, eye.y, eye.z}},
                      .epoch = _world.snapshot().epoch(),
                      .pitch = -.3f,
                      .unitsPerSecond = radius * .15f};
    if (_kind == DemoKind::Bistro || _kind == DemoKind::Benchmark)
      _initialCamera = bistroView(_space, _world.snapshot().epoch());
    _freeCamera.setProps(_initialCamera);
    if (_kind == DemoKind::Chess) {
      const auto center = (box.minimum + box.maximum) * .5f;
      _camera.setProps({.target = {_space, {center.x, center.y, center.z}},
                        .epoch = _world.snapshot().epoch(),
                        .pitch = .55,
                        .distance = radius * 1.7,
                        .minimumDistance = .02,
                        .maximumDistance = std::max(1000., double(radius) * 10),
                        .lens = {.nearPlane = .02f, .farPlane = 5000}});
    }
  }
  _director.setFallback(camera());
  _interactiveCamera = _director.add({.camera = camera()});
  if (_kind == DemoKind::Benchmark)
    _pathCamera = _director.add({.camera = _tour->sample(0), .priority = 10});
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
              (inspecting()
                   ? std::string{"MMB: orbit | Shift+MMB: pan | "
                                 "Ctrl+MMB/wheel: zoom | "
                                 "Alt+LMB: MMB alternative | R: reset pan"} +
                         (_kind == DemoKind::Material ? " | P: smoke" : "")
                   : std::string{"WASD: fly | Arrows: look | Space/Left Shift: "
                                 "rise/fall | F: follow/free | R: reset | RMB: "
                                 "engage/unlock"}) +
              " | Esc: cancel/settings | Q/E: exposure | L: light" +
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
  ui::SceneViewProps props{.preferredSize = {900, 650}};
  project(props);
  props.lighting.diffuseEnvironment = _resources.environment.diffuse;
  props.lighting.specularEnvironment = _resources.environment.specular;
  props.lighting.brdf = _resources.environment.brdf;
  props.lighting.irradiance = _lighting ? math::Vec3f{3, 3, 3} : math::Vec3f{};
  props.exposure = _exposure;
  props.toneMap = true;
  std::unique_ptr<ui::SceneView> view;
  if (inspecting()) {
    auto inspection = std::make_unique<ui::InspectionView>(
        props, _camera.props(),
        ui::InspectionProps{_preferences.mouseLookSensitivity,
                            _preferences.zoomSensitivity,
                            _preferences.mouseInvertY});
    _inspection = inspection.get();
    _inspectionChanged = inspection->onCameraChanged(
        [this](scene::WorldCamera) { updateView(); });
    view = std::move(inspection);
  } else
    view = std::make_unique<ui::SceneView>(props);
  _view = view.get();
  if (_kind == DemoKind::Benchmark)
    prepareBenchmark(ctx);
  root->append(std::move(view), {.grow = 1});
  _ui.root().setContent(std::move(root));
}

scene::WorldCamera Demo3DApp::camera() const {
  return _followMode ? _follow.camera()
         : inspecting()
             ? (_inspection ? _inspection->camera() : _camera.camera())
             : _freeCamera.camera({.nearPlane = .02f, .farPlane = 5000});
}

void Demo3DApp::project(ui::SceneViewProps &props) {
  if (!_projection)
    _projection = std::make_unique<scene::SceneInstanceProjection>(
        std::vector<scene::SceneInstanceBinding>{{_scenery, _scene}}, _ledger);
  props.scene.reset();
  props.worldScene =
      _projection->extract(_world.snapshot(), _director.camera(),
                           {_director.camera().pose.position, 1});
}

void Demo3DApp::updateView() {
  if (!_view)
    return;
  auto props = _view->props();
  _director.update(_interactiveCamera, {.camera = camera()});
  project(props);
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
  if (_kind == DemoKind::Material && actions["pause"].pressed)
    _playback.setPaused(!_playback.isPaused());
  if (actions["light"].pressed)
    _lighting = !_lighting;
  if (actions["exposure-up"].pressed)
    _exposure = std::min(16.f, _exposure * 1.25f);
  if (actions["exposure-down"].pressed)
    _exposure = std::max(.0625f, _exposure / 1.25f);
  if (inspecting()) {
    updateView();
    return;
  }
  _navigation = {};
  _keyboardLook = _gamepadLook = _gamepadMove = {};
  const auto keyboard = input::InputDevice{input::DeviceKind::Keyboard, 0};
  const auto mouse = input::InputDevice{input::DeviceKind::Mouse, 0};
  const float movement = actions["forward"].held || actions["backward"].held ||
                         actions["strafe-left"].held ||
                         actions["strafe-right"].held || actions["rise"].held ||
                         actions["fall"].held;
  const auto rawKey = [&](SDL_Scancode code) {
    return input().physicalValue(input::ControlKind::Key, code);
  };
  const float rawMovement = rawKey(SDL_SCANCODE_W) || rawKey(SDL_SCANCODE_S) ||
                            rawKey(SDL_SCANCODE_A) || rawKey(SDL_SCANCODE_D) ||
                            rawKey(SDL_SCANCODE_SPACE) ||
                            rawKey(SDL_SCANCODE_LSHIFT);
  if (_controls.accept(input::ViewChannel::Movement, keyboard, rawMovement,
                       movement > 0))
    _navigation = actions;
  const math::Vec2f keys{
      float(actions["right"].held) - float(actions["left"].held),
      float(actions["up"].held) - float(actions["down"].held)};
  const float rawLook = rawKey(SDL_SCANCODE_LEFT) ||
                        rawKey(SDL_SCANCODE_RIGHT) || rawKey(SDL_SCANCODE_UP) ||
                        rawKey(SDL_SCANCODE_DOWN);
  if (_controls.accept(input::ViewChannel::Look, keyboard, rawLook,
                       keys != math::Vec2f{}))
    _keyboardLook = keys;
  if (_gamepad) {
    const input::InputDevice pad{input::DeviceKind::Gamepad, *_gamepad};
    const auto move = actions["pad-move"].value,
               look = actions["pad-look"].value;
    const auto raw = [&](SDL_GamepadAxis axis) {
      return input().physicalValue(
          {input::ControlKind::GamepadAxis, axis, *_gamepad});
    };
    if (_controls.accept(input::ViewChannel::Movement, pad,
                         std::hypot(raw(SDL_GAMEPAD_AXIS_LEFTX),
                                    raw(SDL_GAMEPAD_AXIS_LEFTY)),
                         move != math::Vec2f{})) {
      _gamepadMove = input::stickResponse(move, _preferences);
      _navigation = {};
    }
    if (_controls.accept(input::ViewChannel::Look, pad,
                         std::hypot(raw(SDL_GAMEPAD_AXIS_RIGHTX),
                                    raw(SDL_GAMEPAD_AXIS_RIGHTY)),
                         look != math::Vec2f{})) {
      const auto response = input::stickResponse(look, _preferences);
      _gamepadLook = {float(response.x * _preferences.stickLookSpeed),
                      float(response.y * _preferences.stickLookSpeed *
                            (_preferences.stickInvertY ? -1 : 1))};
      _keyboardLook = {};
    }
  }
  if (ctx.windowServices().relativeMouseActive() &&
      _controls.state().pointerLocked &&
      _controls.accept(input::ViewChannel::Look, mouse,
                       std::hypot(actions["pointer-look"].value.x,
                                  actions["pointer-look"].value.y))) {
    _keyboardLook = _gamepadLook = {};
    const math::Vec2f delta{float(actions["pointer-look"].value.x *
                                  _preferences.mouseLookSensitivity),
                            float(actions["pointer-look"].value.y *
                                  _preferences.mouseLookSensitivity *
                                  (_preferences.mouseInvertY ? 1 : -1))};
    if (_followMode)
      _look.advance({.deltaRadians = delta}, 0);
    else
      _freeCamera.advance({.radians = delta}, 0);
  }
  if (_controls.accept(input::ViewChannel::Zoom, mouse,
                       std::abs(actions["wheel-zoom"].value.y))) {
    const double zoom =
        -actions["wheel-zoom"].value.y * .25 * _preferences.zoomSensitivity;
    if (_followMode && _poses)
      _follow.advance(scene::cameraTarget(_poses->current()), _look.state(),
                      {zoom, 0}, 0);
    else {
      auto props = _freeCamera.props();
      props.unitsPerSecond = std::clamp(
          props.unitsPerSecond * std::exp(actions["wheel-zoom"].value.y * .1),
          .1, 100.);
      _freeCamera.setProps(props);
    }
  }
  if (actions["follow"].pressed && _view && _kind == DemoKind::Bistro) {
    if (!_followMode) {
      auto props = _world.snapshot().resolve(_world.handle(_subject)).props;
      props.pose = _freeCamera.camera().pose;
      props.pose.position =
          world::translated(props.pose.position, {0, -1.7, 0});
      props.pose.orientation =
          scene::LookController{{_freeCamera.props().yaw, 0}}.orientation();
      props.velocity = {_space};
      const std::array<world::WorldMutation, 1> change{
          world::SetEntity{_subject, props, true}};
      _world.apply(change, _world.snapshot().version(),
                   _world.snapshot().tick());
      _poses.emplace(_world.snapshot().sample(_world.handle(_subject)));
      _look.setState({_freeCamera.props().yaw, _freeCamera.props().pitch});
      _follow.reset(scene::cameraTarget(_poses->current()), _look.state());
      _followMode = true;
    } else {
      auto props = _freeCamera.props();
      props.position = _follow.camera().pose.position;
      props.yaw = _look.state().yaw;
      props.pitch = _look.state().pitch;
      _freeCamera.setProps(props);
      _followMode = false;
    }
    ctx.setSimulationPaused(!_followMode);
    _controls.suspend(input::ControlReason::Target);
    _navigation = {};
    _keyboardLook = _gamepadLook = _gamepadMove = {};
    _facing.reset();
  }
  if (actions["reset"].pressed && _view && !_followMode &&
      _kind != DemoKind::Material)
    _freeCamera.setProps(_initialCamera);
  updateView();
}

EventResult Demo3DApp::handleEvent(AppContext &ctx, const SDL_Event &event) {
  synchronize(ctx);
  if (_kind == DemoKind::Benchmark) {
    if (_benchmark.traversing() &&
        (event.type == SDL_EVENT_WINDOW_RESIZED ||
         event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED))
      _benchmark.invalidate(runtime::BenchmarkInterruption::Window);
    return _ui.handleEvent(event);
  }
  auto &window = ctx.windowServices();
  _controls.synchronize(window.focused(), _ui.inputClaims());
  if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE &&
      window.relativeMouseActive()) {
    _controls.release();
    window.releaseRelativeMouse();
    input().cancelAll();
    _navigation = {};
    return EventResult::Consumed;
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
      event.button.button == SDL_BUTTON_RIGHT &&
      _preferences.mouseCapture == input::MouseCapture::Hold &&
      _controls.state().pointerLocked) {
    _controls.release();
    _navigation = {};
    return EventResult::Consumed;
  }
  if (event.type == SDL_EVENT_KEYBOARD_REMOVED)
    _controls.suspend(
        input::ControlReason::Device,
        std::array{input::ViewChannel::Movement, input::ViewChannel::Look});
  if (event.type == SDL_EVENT_MOUSE_REMOVED)
    _controls.suspend(
        input::ControlReason::Device,
        std::array{input::ViewChannel::Look, input::ViewChannel::Zoom});
  if (event.type == SDL_EVENT_GAMEPAD_REMOVED &&
      _gamepad == event.gdevice.which) {
    _controls.removeDevice({input::DeviceKind::Gamepad, *_gamepad});
    if (_gamepadContext)
      input().removeContext(*_gamepadContext);
    _gamepadContext.reset();
    _gamepad.reset();
    _gamepadLook = _gamepadMove = {};
  }
  if ((event.type == SDL_EVENT_MOUSE_MOTION ||
       event.type == SDL_EVENT_MOUSE_WHEEL) &&
      window.relativeMouseActive() && _controls.state().pointerLocked)
    return EventResult::Ignored; // InputMap receives the displacement once,
                                 // without UI hover/scroll.
  const auto result = _ui.handleEvent(event);
  if (inspecting())
    return result;
  if (result != EventResult::Ignored)
    return result;
  if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
      event.gbutton.button == SDL_GAMEPAD_BUTTON_START && _view &&
      !_ui.inputClaims().gamepad && window.focused()) {
    if (_gamepad != event.gbutton.which) {
      if (_gamepadContext)
        input().removeContext(*_gamepadContext);
      _gamepad = event.gbutton.which;
      _gamepadContext =
          input().addContext({.name = "view-gamepad"},
                             {{.action = "pad-move",
                               .kind = input::ActionKind::Vector,
                               .control = input::ControlKind::GamepadAxis,
                               .code = SDL_GAMEPAD_AXIS_LEFTX,
                               .device = *_gamepad,
                               .contribution = {1, 0}},
                              {.action = "pad-move",
                               .kind = input::ActionKind::Vector,
                               .control = input::ControlKind::GamepadAxis,
                               .code = SDL_GAMEPAD_AXIS_LEFTY,
                               .device = *_gamepad,
                               .contribution = {0, -1}},
                              {.action = "pad-look",
                               .kind = input::ActionKind::Vector,
                               .control = input::ControlKind::GamepadAxis,
                               .code = SDL_GAMEPAD_AXIS_RIGHTX,
                               .device = *_gamepad,
                               .contribution = {1, 0}},
                              {.action = "pad-look",
                               .kind = input::ActionKind::Vector,
                               .control = input::ControlKind::GamepadAxis,
                               .code = SDL_GAMEPAD_AXIS_RIGHTY,
                               .device = *_gamepad,
                               .contribution = {0, -1}}});
      _controls.assign({{input::DeviceKind::Keyboard, 0},
                        {input::DeviceKind::Mouse, 0},
                        {input::DeviceKind::Gamepad, *_gamepad}});
    }
    _controls.engage({ctx.activationToken(),
                      {input::DeviceKind::Gamepad, *_gamepad},
                      window.focused(),
                      true,
                      false,
                      {}});
    return EventResult::Consumed;
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
      event.button.button == SDL_BUTTON_RIGHT && _view && _view->viewport() &&
      !_ui.inputClaims().pointer &&
      _view->viewport()->normalizedPosition(
          _ui.mapping().toLogical({event.button.x, event.button.y}))) {
    if (window.relativeMouseActive())
      _controls.release();
    else {
      try {
        _controls.engage(
            {ctx.activationToken(),
             {input::DeviceKind::Mouse, 0},
             window.focused(),
             true,
             true,
             [&] {
               return std::make_shared<ui::Connection>(
                   window.lockRelativeMouse(ctx.activationToken()));
             }});
      } catch (const std::exception &error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", error.what());
      }
    }
    return EventResult::Consumed;
  }
  return result;
}

runtime::ActivityProps Demo3DApp::activityProps() const {
  if (!_view)
    return {false, false};
  const bool animation = _kind == DemoKind::Material && !_playback.isPaused();
  const bool benchmark = _kind == DemoKind::Benchmark && !_benchmarkReported;
  bool movement = _followMode || _keyboardLook != math::Vec2f{} ||
                  _gamepadLook != math::Vec2f{} ||
                  _gamepadMove != math::Vec2f{};
  for (const auto *action :
       {"forward", "backward", "strafe-left", "strafe-right", "rise", "fall"})
    movement |= _navigation[action].held;
  return {animation || benchmark || (!inspecting() && movement),
          animation ||
              (_kind == DemoKind::Benchmark && _benchmark.traversing())};
}

runtime::ActivityDemand Demo3DApp::activityDemand() {
  auto demand = _ui.activityDemand();
  demand.update |= _preparation.isReady() || (_loadRequested && !_loadRetryAt);
  if (_loadRequested && _loadRetryAt &&
      (!demand.wakeAt || *_loadRetryAt < *demand.wakeAt))
    demand.wakeAt = _loadRetryAt;
  return demand;
}

void Demo3DApp::update(AppContext &ctx, float dt) {
  applyControls(ctx);
  if (_loadRequested || _preparation.isReady()) {
    try {
      if (_loadRequested &&
          (!_loadRetryAt || runtime::ActivityClock::now() >= *_loadRetryAt)) {
        const auto admission = _preparation.start(
            ctx.workers(), ctx.resources(), _preparationProps,
            [sink = ctx.completions()] { sink.post([] {}); });
        if (admission == runtime::TaskAdmission::Busy) {
          _loadRetryAt =
              runtime::ActivityClock::now() + std::chrono::milliseconds{50};
        } else if (admission != runtime::TaskAdmission::Accepted) {
          throw std::runtime_error(std::string{runtime::describe(admission)});
        } else {
          _loadRequested = false;
          _loadRetryAt.reset();
        }
      }
      if (auto result = _preparation.poll()) {
        if (result->error)
          std::rethrow_exception(result->error);
        auto prepared = ctx.resources().publish(std::move(result->assets),
                                                _preparationProps);
        Resources resources;
        for (const auto &model : prepared.models)
          resources.models.push_back(model.model);
        resources.environment = prepared.environment->lighting;
        if (_kind == DemoKind::Material) {
          resources.smoke = prepared.textures.front().second;
          resources.environmentReference =
              std::make_shared<const rendering::Texture>(
                  rendering::TextureRole::Color,
                  prepared.environment->source->levels());
        }
        createView(ctx, std::move(resources));
      }
    } catch (const runtime::ResourcePressure &error) {
      preparationError(ctx, error.what());
      if (!_reclaimedPreparation && error.requested <= error.limit) {
        _reclaimedPreparation = true;
        ctx.reclaimResources();
        _loadRequested = true;
        _loadRetryAt =
            runtime::ActivityClock::now() + std::chrono::milliseconds{50};
      }
    } catch (const std::exception &error) {
      preparationError(ctx, error.what());
    }
  }

  if (_view && _kind == DemoKind::Benchmark)
    updateBenchmark(ctx);
  if (_view && !_followMode && !inspecting() && _kind != DemoKind::Benchmark) {
    auto held = [&](const char *key) { return float(_navigation[key].held); };
    _freeCamera.advance(
        {.movement = {held("strafe-right") - held("strafe-left") +
                          _gamepadMove.x,
                      held("rise") - held("fall"),
                      held("forward") - held("backward") + _gamepadMove.y},
         .radians = {(_keyboardLook.x + _gamepadLook.x) * dt,
                     (_keyboardLook.y + _gamepadLook.y) * dt}},
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
    props.transform = scene::billboard(
        {1, -1, 0}, camera().localCamera({{_space, {}}, 1}, {}),
        {1.5f, 1.5f, 1.5f});
    const auto &previous = _scene->props(_smoke);
    if (previous.transform != props.transform ||
        previous.material.colorTexture.transform !=
            props.material.colorTexture.transform)
      _scene->setProps(_smoke, std::move(props));
    updateView();
  }
  if (_followMode && _poses) {
    _look.advance({.radiansPerSecond = _keyboardLook + _gamepadLook}, dt);
    _follow.advance(
        scene::cameraTarget(_poses->sample(ctx.simulationState()->alpha)),
        _look.state(), {}, dt);
    updateView();
  }
  _ui.update(dt);
}

void Demo3DApp::preparationError(AppContext &ctx, std::string_view message) {
  _preparation.cancel();
  _loadRequested = false;
  _loadRetryAt.reset();
  _view = nullptr;
  _inspection = nullptr;
  _inspectionChanged.disconnect();
  _caption = nullptr;
  _projection.reset();
  _scene.reset();
  _resources = {};
  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Scene preparation failed: %.*s",
              int(message.size()), message.data());
  _ui.root().setContent(std::make_unique<ui::Text>(
      ctx.assets(),
      ui::TextProps{.value = std::string{"Scene preparation failed: "} +
                             std::string{message},
                    .font = ctx.resources().font(app::fontAsset),
                    .wrap = ui::TextWrap::AvailableInlineSize,
                    .textRole = ui::TextRole::Body,
                    .ink = ui::TextInk::Error}));
}

void Demo3DApp::applyControls(AppContext &ctx) {
  const auto preferences = ctx.controlsState().requested;
  if (_inspection)
    _inspection->setNavigationProps({preferences.mouseLookSensitivity,
                                     preferences.zoomSensitivity,
                                     preferences.mouseInvertY});
  if (preferences != _preferences) {
    _controls.suspend(input::ControlReason::Explicit);
    _navigation = {};
    _keyboardLook = _gamepadLook = _gamepadMove = {};
    _facing.reset();
    _preferences = preferences;
  }
  _controls.setThresholds(
      float(preferences.stickInnerDeadZone),
      float(preferences.stickInnerDeadZone +
            std::min(.1, (1 - preferences.stickInnerDeadZone) * .5)));
  auto locomotion = preferences.steering == input::SteeringRecipe::Responsive
                        ? world::LocomotionProps::responsive()
                        : world::LocomotionProps::outOfNowhere();
  locomotion.profile =
      static_cast<world::LocomotionProfile>(preferences.locomotion);
  _facing.setProps(locomotion);
  auto follow = _follow.props();
  follow.distance = _follow.state().requestedDistance;
  follow.perspective =
      static_cast<scene::PerspectivePolicy>(preferences.perspective);
  follow.reducedMotion = _ui.root().motion().effectivePreference() !=
                         runtime::MotionPreference::Full;
  if (follow.perspective != _follow.props().perspective ||
      follow.reducedMotion != _follow.props().reducedMotion)
    _follow.setProps(follow);
}

void Demo3DApp::fixedUpdate(AppContext &, runtime::SimulationStep step,
                            const input::InputSnapshot &actions) {
  if (!_followMode || !_poses)
    return;
  const auto actual = _world.snapshot().sample(_world.handle(_subject));
  const double right = double(_navigation["strafe-right"].held) -
                       double(_navigation["strafe-left"].held) + _gamepadMove.x;
  const double forward = double(_navigation["forward"].held) -
                         double(_navigation["backward"].held) + _gamepadMove.y;
  auto request = _facing.advance(actual,
                                 {.right = std::clamp(right, -1., 1.),
                                  .forward = std::clamp(forward, -1., 1.),
                                  .turn = std::clamp(right, -1., 1.),
                                  .viewHeading = _look.state().yaw,
                                  .aim = actions["aim"].held,
                                  .freeLook = actions["free-look"].held,
                                  .recenter = actions["recenter"].pressed},
                                 step.seconds);
  const auto moved = world::realizeMovement(actual, request, step.seconds);
  if (moved.actual.pose == actual.pose &&
      moved.actual.velocity == actual.velocity) {
    // Finish interpolation without publishing an unchanged world generation.
    _poses->teleport(actual);
    return;
  }
  auto props = _world.snapshot().resolve(actual.entity).props;
  props.pose = moved.actual.pose;
  props.velocity = moved.actual.velocity;
  const std::array<world::WorldMutation, 1> change{
      world::SetEntity{_subject, std::move(props)}};
  _world.apply(change, _world.snapshot().version(), step.tick);
  _poses->publish(_world.snapshot().sample(_world.handle(_subject)));
}

void Demo3DApp::configureLaunch(const AppLaunchProps &props) {
  if (_kind != DemoKind::Benchmark) {
    IApp::configureLaunch(props);
    return;
  }
  _benchmark = runtime::BenchmarkRun{props.benchmarkSeconds.value_or(15)};
}

scene::CameraPath Demo3DApp::benchmarkPath() const {
  auto shot = [&](scene::CameraProps props) {
    return scene::WorldCamera::fromLocal(props, {{_space, {}}, 1},
                                         _world.snapshot().epoch());
  };
  const scene::CameraPath path{
      15,
      {{0, shot({.eye = {24.82285f, 3.16055f, 61.64814f},
                 .target = {24.50443f, 3.10232f, 60.70198f},
                 .nearPlane = .02f,
                 .farPlane = 5000})},
       {5, shot({.eye = {22, 3.3f, 53},
                 .target = {20, 3.1f, 48},
                 .nearPlane = .02f,
                 .farPlane = 5000})},
       {10, shot({.eye = {20, 3.4f, 45},
                  .target = {24, 3.1f, 57},
                  .nearPlane = .02f,
                  .farPlane = 5000})}}};
  return path;
}

void Demo3DApp::restartBenchmark(AppContext &ctx) {
  _benchmark = runtime::BenchmarkRun{_benchmark.duration()};
  prepareBenchmark(ctx);
}

void Demo3DApp::prepareBenchmark(AppContext &ctx) {
  _benchmarkSubmitted = ctx.renderRuntimeState().submitted;
  _seenCPU = ctx.renderRuntimeState().cpuSamples;
  _seenGPU.clear();
  _benchmarkReported = false;
  _caption->setBoxProps({.height = layout::SizeRule::fixed(64)});
  _measuredSubmitted = ctx.renderRuntimeState().submitted;
  _finalSubmitted = 0;
  _benchmarkTargetPixels = {};
  _benchmarkWork = ctx.renderRuntimeState().sceneWork;
  _director.update(_pathCamera, {.camera = _tour->sample(0), .priority = 10});
  auto props = _view->props();
  project(props);
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
  if (_view->viewport()) {
    const auto pixels = _view->viewport()->pixelSize;
    if (_benchmark.phase() == BenchmarkPhase::Measuring &&
        pixels != _benchmarkTargetPixels)
      _benchmark.invalidate(runtime::BenchmarkInterruption::Window);
    if (!_benchmark.firstFrame)
      _benchmarkTargetPixels = pixels;
  }
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
  if (_benchmark.traversing()) {
    if (ctx.graphicsState().requested != _benchmarkGraphics)
      _benchmark.invalidate(runtime::BenchmarkInterruption::Graphics);
    else if (ctx.windowState().drawableSize != _benchmarkPixels)
      _benchmark.invalidate(runtime::BenchmarkInterruption::Window);
    else if (state.gpu.domain != _benchmarkDomain)
      _benchmark.invalidate(runtime::BenchmarkInterruption::Device);
  }
  if (_benchmark.phase() == BenchmarkPhase::Measuring &&
      ctx.graphicsState().sceneScale != _benchmarkScale)
    _benchmark.invalidate(runtime::BenchmarkInterruption::Quality);
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
    _director.update(
        _pathCamera,
        {.camera = _tour->sample(_benchmark.elapsed(now)), .priority = 10});
    auto props = _view->props();
    project(props);
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
  const auto pixels = _benchmarkTargetPixels;
  auto message = std::format(
      "BenchmarkJSON "
      "{{\"scene\":\"demoscene.bistro@1\",\"path\":\"bistro-street-v1\","
      "\"phase\":\"{}\",\"interruption\":\"{}\",\"final\":{},\"requested_"
      "seconds\":{},\"measured_"
      "seconds\":{:.6f},\"load_seconds\":{:.6f},\"warmup_seconds\":15,"
      "\"domain\":{},\"width\":{},\"height\":{},\"vsync\":{},\"scene_scale\":{}"
      ",\"cpu_render_samples\":{},\"cpu_render_p50_ms\":{:.6f},\"cpu_render_"
      "p95_ms\":{:.6f},\"cpu_render_p99_ms\":{:.6f},\"cpu_render_max_ms\":{:."
      "6f},\"gpu_scene_samples\":{},\"gpu_scene_mean_ms\":{:.6f},\"uploads\":{}"
      ",\"upload_bytes\":{},\"cpu_omitted\":{},\"drain_timed_out\":{}}}",
      runtime::toString(_benchmark.phase()),
      runtime::toString(_benchmark.interruption), final, _benchmark.duration(),
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
      "\"pending_gpu_queries\":{},\"managed_cpu_peak_bytes\":{},\"managed_gpu_"
      "peak_bytes\":{},\"cpu_"
      "phases\":{{",
      build, SDL_GetPlatform(),
      (_finalSubmitted ? _finalSubmitted : state.submitted) -
          _measuredSubmitted,
      state.gpu.queryDrops >= _queryDrops ? state.gpu.queryDrops - _queryDrops
                                          : 0,
      state.gpu.bufferDiscards >= _bufferDiscards
          ? state.gpu.bufferDiscards - _bufferDiscards
          : 0,
      state.gpu.pending, state.resources.memory[0].peak,
      state.resources.memory[1].peak);
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
             "Bistro benchmark: {} ({}) | CPU render p50 {:.2f}ms / p95 "
             "{:.2f}ms | "
             "GPU scene {:.2f}ms ({} samples) | uploads {} | R: restart",
             runtime::toString(_benchmark.phase()),
             runtime::toString(_benchmark.interruption), cpu.percentile(.5),
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
  _controls.release();
  if (_pathCamera)
    _director.remove(std::exchange(_pathCamera, 0));
  if (_interactiveCamera)
    _director.remove(std::exchange(_interactiveCamera, 0));
  _preparation.cancel();
  _loadRequested = false;
  _loadRetryAt.reset();
  _navigation = {};
  _ui.clear();
  _view = nullptr;
  _caption = nullptr;
  _inspectionChanged.disconnect();
  _inspection = nullptr;
  _projection.reset();
  _scene.reset();
  _resources = {};
}
} // namespace playground::demo3d
