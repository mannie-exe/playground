#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL3/SDL_scancode.h>

#include <minesweeper/MinesweeperApp.hpp>

using namespace playground;
namespace mineConfig = playground::minesweeper::config;

namespace {
ActionButtonProps actionStyle() {
  return {.button = {.normal = mineConfig::buttonBaseColor,
                     .hover = mineConfig::buttonHoverColor,
                     .pressed = mineConfig::buttonActiveColor,
                     .useTheme = false},
          .labelColor = mineConfig::actionLabelColor};
}

MinesweeperUIProps gameProps(const MinesweeperBoardProps &board) {
  return {.grid = {.size = board.size,
                   .cellSize = mineConfig::cellSize,
                   .gap = mineConfig::gridGap},
          .cell = {.bombColor = mineConfig::bombBgColor,
                   .revealedColor = mineConfig::revealedBgColor,
                   .clearedColor = mineConfig::buttonClearedColor,
                   .flagColor = mineConfig::flagCounterIconColor,
                   .iconPadding = mineConfig::iconPadding,
                   .labelColors = mineConfig::cellLabelColors,
                   .button = actionStyle().button},
          .actionButton = actionStyle(),
          .flagCounter = {.button = {.disabled = mineConfig::buttonBaseColor,
                                     .useTheme = false},
                          .iconColor = mineConfig::flagCounterIconColor,
                          .labelColor = mineConfig::flagCounterLabelColor},
          .layout = {.padding = mineConfig::outerPadding,
                     .footerHeight = mineConfig::footerHeight,
                     .footerCounterWidth = mineConfig::footerCounterWidth,
                     .footerGap = mineConfig::footerGap,
                     .actionWidth = mineConfig::actionWidth}};
}

ui::Node *focusedNode(ui::Node &node) {
  if (node.hasFocus())
    return &node;
  for (const auto &child : node.children())
    if (auto *focused = focusedNode(*child))
      return focused;
  return nullptr;
}
} // namespace

AppInfo MinesweeperApp::staticInfo() {
  return {.id = AppId::Minesweeper,
          .name = mineConfig::gameName,
          .window = {.title = std::string{mineConfig::windowTitle},
                     .clearColor = mineConfig::bgColor},
          .view = mineConfig::viewPolicy,
          .presentation = {.window = {.mode = mineConfig::windowMode}}};
}

MinesweeperApp::MinesweeperApp() {
  input().addContext(
      {.name = "navigation", .stage = input::InputStage::BeforeUI},
      {{.action = "back", .code = SDL_SCANCODE_ESCAPE}});
}

void MinesweeperApp::onActions(AppContext &ctx,
                               const input::InputSnapshot &actions) {
  if (actions["back"].pressed) {
    _pending =
        _screen == Screen::Game ? Command::Difficulty : Command::Launcher;
    processActions(ctx);
  }
}

std::optional<math::Size2>
MinesweeperApp::preferredContentSize(math::Size2 maximum, math::Vec2f density) {
  return _session.root().preferredSize(maximum, density);
}

void MinesweeperApp::installView(std::unique_ptr<ui::Node> content) {
  auto scroll = std::make_unique<ui::ScrollView>(
      std::move(content), ui::ScrollProps{.axes = ui::ScrollAxes::Both,
                                          .sizing = ui::ScrollSizing::Content});
  auto *pointer = scroll.get();
  _session.root().setContent(std::move(scroll));
  _scroll = pointer->handle<ui::ScrollView>();
  _focusPending = true;
}

void MinesweeperApp::showDifficulty() {
  auto menu = std::make_unique<DifficultyUI>(
      _draft, *_resources, actionStyle(),
      [this](MinesweeperBoardProps board) {
        _requestedBoard = board;
        _pending = Command::Start;
      },
      [this] { _pending = Command::Launcher; });
  auto *pointer = menu.get();
  installView(std::move(menu));
  _menu = pointer->handle<DifficultyUI>();
  _view = {};
  _model.reset();
  _screen = Screen::Difficulty;
}

void MinesweeperApp::startGame(MinesweeperBoardProps board) {
  board.validate();
  if (board.size.x < mineConfig::minimumDimension ||
      board.size.y < mineConfig::minimumDimension ||
      board.size.x > mineConfig::maximumDimension ||
      board.size.y > mineConfig::maximumDimension || board.bombs < 1)
    throw std::invalid_argument("Board exceeds custom difficulty limits");
  auto model = std::make_unique<MinesweeperModel>(board);
  auto view =
      std::make_unique<MinesweeperUI>(gameProps(board), *model, *_resources);
  auto *pointer = view.get();
  installView(std::move(view));
  _model = std::move(model);
  _view = pointer->handle<MinesweeperUI>();
  _menu = {};
  _screen = Screen::Game;
}

void MinesweeperApp::onEnter(AppContext &ctx) {
  _resources.emplace(minesweeper::acquireResources(ctx.resources()));
  showDifficulty();
  synchronize(ctx);
}

void MinesweeperApp::onExit(AppContext &) {
  _session.clear();
  _view = {};
  _menu = {};
  _scroll = {};
  _pending.reset();
  _model.reset();
  _resources.reset();
}

void MinesweeperApp::rebuildView() {
  if (!_resources)
    throw std::logic_error("Enter app before rebuilding its view");
  if (_screen == Screen::Difficulty) {
    showDifficulty();
    return;
  }
  auto view = std::make_unique<MinesweeperUI>(gameProps(_model->props()),
                                              *_model, *_resources);
  auto *pointer = view.get();
  installView(std::move(view));
  _view = pointer->handle<MinesweeperUI>();
}

void MinesweeperApp::processActions(AppContext &ctx) {
  if (auto *view = _view.get()) {
    view->processPendingActions();
    if (view->isMenuRequested())
      _pending = Command::Difficulty;
  }
  if (!_pending)
    return;
  const auto command = std::exchange(_pending, {});
  if (*command == Command::Launcher) {
    ctx.requestMenu();
    return;
  }
  try {
    if (*command == Command::Start)
      startGame(_requestedBoard);
    else
      showDifficulty();
    ctx.requestFitContent();
  } catch (const std::exception &error) {
    if (auto *menu = _menu.get())
      menu->setStatus(error.what());
    else
      throw;
  }
}

void MinesweeperApp::revealFocus() {
  if (auto *scroll = _scroll.get())
    if (auto *focus = focusedNode(*scroll))
      scroll->scrollIntoView(*focus);
}

void MinesweeperApp::synchronize(AppContext &ctx) {
  _session.synchronize(ctx);
  if (_focusPending) {
    _session.root().focusNext();
    _focusPending = false;
  }
}

EventResult MinesweeperApp::handleEvent(AppContext &ctx,
                                        const SDL_Event &event) {
  _session.update(0);
  synchronize(ctx);
  const auto result = _session.handleEvent(event);
  processActions(ctx);
  if (event.type == SDL_EVENT_KEY_DOWN) {
    synchronize(ctx);
    revealFocus();
  }
  return result;
}

void MinesweeperApp::update(AppContext &ctx, float dt) {
  _session.update(dt);
  synchronize(ctx);
  processActions(ctx);
}

void MinesweeperApp::render(AppContext &ctx, rendering::RenderFrame &frame) {
  synchronize(ctx);
  _session.render(frame);
}
