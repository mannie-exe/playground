#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <app/Assets.hpp>
#include <menu/Config.hpp>
#include <menu/MenuApp.hpp>

using namespace playground;

MenuApp::MenuApp() {
  std::vector<input::Binding> bindings;
  for (const auto &entry : menu::entries)
    bindings.push_back(
        {.action = std::string{appKey(entry.app)}, .code = entry.key});
  bindings.push_back({.action = "quit", .code = SDL_SCANCODE_Q});
  input().addContext({.name = "launcher", .stage = input::InputStage::BeforeUI},
                     std::move(bindings));
}

AppInfo MenuApp::staticInfo() {
  return {
      .id = AppId::Menu,
      .name = "Menu",
      .window = {.title = "Me n' U", .clearColor = {24, 24, 24, 255}},
      .view = {.initialSizing = platform::InitialWindowSizing::FitContent,
               .minimumSize = {menu::contentWidth + 2 * menu::padding, 420}}};
}

void MenuApp::synchronize(AppContext &ctx) { _ui.synchronize(ctx); }

void MenuApp::launchPending(AppContext &ctx) {
  if (const auto target = std::exchange(_pending, std::nullopt))
    ctx.requestSwitch(*target, std::exchange(_launch, {}));
}

void MenuApp::onEnter(AppContext &ctx) {
  auto view = std::make_unique<menu::MenuUI>(
      ctx.assets(),
      ctx.resources().font(app::fontAsset, {.style = {.size = 22}}),
      ctx.resources().font(app::fontAsset, {.style = {.size = 34}}),
      [this](AppId app, AppLaunchProps launch) {
        _pending = app;
        _launch = launch;
      });
  _view = view.get();
  _ui.root().setContent(std::move(view));
  synchronize(ctx);
}

void MenuApp::onExit(AppContext &) {
  _ui.clear();
  _view = nullptr;
  _pending.reset();
}

std::optional<math::Size2> MenuApp::preferredContentSize(math::Size2 maximum,
                                                         math::Vec2f density) {
  return _ui.root().preferredSize(maximum, density);
}

void MenuApp::onActions(AppContext &ctx, const input::InputSnapshot &actions) {
  if (actions["quit"].pressed) {
    _pending.reset();
    ctx.requestQuit();
    return;
  }
  for (const auto &entry : menu::entries)
    if (actions[appKey(entry.app)].pressed) {
      _pending = entry.app;
      break;
    }
  launchPending(ctx);
}

EventResult MenuApp::handleEvent(AppContext &ctx, const SDL_Event &event) {
  synchronize(ctx);
  const auto result = _ui.handleEvent(event);
  launchPending(ctx);
  return result;
}

void MenuApp::update(AppContext &ctx, float dt) {
  if (_view)
    _view->setStatus(ctx.lastCommandError());
  _ui.update(dt);
  synchronize(ctx);
  launchPending(ctx);
}

void MenuApp::render(AppContext &ctx, rendering::RenderFrame &frame) {
  synchronize(ctx);
  _ui.render(frame);
}
