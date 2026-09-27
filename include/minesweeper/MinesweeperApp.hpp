#pragma once

#include <memory>
#include <optional>

#include <app/IApp.hpp>
#include <minesweeper/Config.hpp>
#include <minesweeper/DifficultyUI.hpp>
#include <minesweeper/MinesweeperUI.hpp>
#include <platform/sdl/UISession.hpp>
#include <ui/collections/ScrollView.hpp>

class MinesweeperApp : public IApp {
  enum class Screen { Difficulty, Game };
  enum class Command { Start, Difficulty, Launcher };

  MinesweeperBoardProps _draft{playground::minesweeper::config::easy};
  MinesweeperBoardProps _requestedBoard{_draft};
  std::unique_ptr<MinesweeperModel> _model;
  std::optional<playground::minesweeper::ViewResources> _resources;

  playground::sdl::UISession _session{
      playground::sdl::UISessionTiming::Monotonic};
  playground::ui::NodeHandle<MinesweeperUI> _view;
  playground::ui::NodeHandle<DifficultyUI> _menu;
  playground::ui::NodeHandle<playground::ui::ScrollView> _scroll;
  Screen _screen{Screen::Difficulty};
  std::optional<Command> _pending;
  bool _focusPending{};

  void showDifficulty();
  void startGame(MinesweeperBoardProps);
  void installView(std::unique_ptr<playground::ui::Node>);
  void processActions(AppContext &);
  void revealFocus();
  void synchronize(AppContext &);

public:
  MinesweeperApp();
  void onActions(AppContext &,
                 const playground::input::InputSnapshot &) override;
  static AppInfo staticInfo();

  AppInfo info() const override { return staticInfo(); }

  playground::runtime::ActivityProps activityProps() const override {
    return {false, false};
  }

  playground::runtime::ActivityDemand activityDemand() override {
    return _session.activityDemand();
  }

  std::optional<playground::math::Size2>
  preferredContentSize(playground::math::Size2 maximum,
                       playground::math::Vec2f density) override;
  void onEnter(AppContext &) override;
  void onExit(AppContext &) override;
  EventResult handleEvent(AppContext &, const SDL_Event &) override;
  void update(AppContext &, float) override;
  void render(AppContext &, playground::rendering::RenderFrame &) override;
  void rebuildView();
};
