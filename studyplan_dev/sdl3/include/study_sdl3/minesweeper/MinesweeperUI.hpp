#pragma once

#include <algorithm>
#include <optional>
#include <utility>

#include <study_sdl3/interfaces/IDisplayObject.hpp>
#include <study_sdl3/minesweeper/FlagCounter.hpp>
#include <study_sdl3/minesweeper/MinesweeperGrid.hpp>
#include <study_sdl3/minesweeper/NewGameButton.hpp>
#include <study_sdl3/platform/DisplayState.hpp>

enum class GameState {
  Playing,
  Won,
  Lost,
};

struct MinesweeperUILayoutProps {
  int padding;
  int footerHeight;
  int footerCounterWidth;
  int footerGap;
};

struct MinesweeperUIProps {
  RectTransform transform;

  DisplayState displayState;

  MinesweeperGridProps grid;
  MinesweeperCellStyle cell;
  NewGameButtonProps newGameButton;
  FlagCounterProps flagCounter;
  MinesweeperUILayoutProps layout;
};

struct MinesweeperUIPropsPatch {
  std::optional<DisplayState> displayState;
  std::optional<GameState> state;
};

class MinesweeperUI : public IDisplayObject {
  study_sdl3::minesweeper::MinesweeperEvents _events;

  MinesweeperUIProps _props;
  SurfaceHandle _bombImage;
  SurfaceHandle _flagImage;
  FontHandle _font;
  std::optional<MinesweeperGrid> _grid;
  NewGameButton _newGameButton;
  FlagCounter _flagCounter;

  GameState _state{GameState::Playing};

public:
  explicit MinesweeperUI(
      const MinesweeperUIProps props,
      const study_sdl3::minesweeper::MinesweeperEvents &events,
      SurfaceHandle bombImage, SurfaceHandle flagImage, FontHandle font)
      : IDisplayObject{props.transform}, _events{events}, _props{props},
        _bombImage{std::move(bombImage)}, _flagImage{std::move(flagImage)},
        _font{std::move(font)},
        _grid{std::in_place, gridTransform(props), props.grid, props.cell,
              events,        _bombImage,           _flagImage, _font},
        _newGameButton{newGameTransform(props), *this, events, _font,
                       props.newGameButton},
        _flagCounter{flagCounterTransform(props), *this, _flagImage, _font,
                     props.flagCounter} {
    _flagCounter.setAmount(_grid->getBombCount());
    relayout();
  }

  bool isPlaying() const { return _state == GameState::Playing; }
  GameState getGameState() const { return _state; }
  const MinesweeperUIProps &getProps() const { return _props; }
  const MinesweeperGrid &getGrid() const { return *_grid; }
  MinesweeperGrid &getGrid() { return *_grid; }

  void setDisplayState(DisplayState displayState) {
    _props.displayState = displayState;
    relayout();
  }

  void setTransform(const RectTransform &transform) override {
    _props.transform = transform;
    IDisplayObject::setTransform(transform);
    _grid->setTransform(gridTransform(_props));
    _newGameButton.setTransform(newGameTransform(_props));
    _flagCounter.setTransform(flagCounterTransform(_props));
  }

  void setGameState(GameState state) { _state = state; }

  void handleWin() { _state = GameState::Won; }
  void handleLoss() { _state = GameState::Lost; }
  void reset() {
    _grid.emplace(gridTransform(_props), _props.grid, _props.cell, _events,
                  _bombImage, _flagImage, _font);
    _state = GameState::Playing;
    _flagCounter.setAmount(_grid->getBombCount());
    relayout();
  }

  void applyPropsPatch(const MinesweeperUIPropsPatch &patch) {
    if (patch.displayState)
      setDisplayState(*patch.displayState);
    if (patch.state)
      setGameState(*patch.state);
  }

  EventResult handleEvent(const SDL_Event &event) override {
    if (!isVisible())
      return EventResult::Ignored;

    if (event.type == _events.gameWon) {
      handleWin();
      return EventResult::Consumed;
    }

    if (event.type == _events.gameLost) {
      handleLoss();
      return EventResult::Consumed;
    }

    if (event.type == _events.newGameRequested) {
      reset();
      return EventResult::Consumed;
    }

    if (event.type == _events.flagToggled) {
      const bool flagged{*static_cast<const bool *>(event.user.data2)};

      if (_flagCounter.getAmount() == 0 && flagged) {
        _grid->getCellAt(*static_cast<const Vec2i *>(event.user.data1))
            .setFlagged(false);
        return EventResult::Consumed;
      }

      _flagCounter.setAmount(_flagCounter.getAmount() + (flagged ? -1 : 1));
      return EventResult::Consumed;
    }

    EventResult result{_newGameButton.handleEvent(event)};
    if (isTerminal(result) || !isPlaying())
      return result;
    result = combine(result, _flagCounter.handleEvent(event));
    if (isTerminal(result))
      return result;
    return combine(result, _grid->handleEvent(event));
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    _grid->render(targetSurface);
    _newGameButton.render(targetSurface);
    _flagCounter.render(targetSurface);
  }

private:
  static RectTransform gridTransform(const MinesweeperUIProps &props) {
    return rect(props.transform.position.x, props.transform.position.y,
                static_cast<float>(props.grid.width()),
                static_cast<float>(props.grid.height()));
  }

  static RectTransform newGameTransform(const MinesweeperUIProps &props) {
    const float y{props.transform.position.y + props.grid.height() +
                  props.layout.padding};
    const float width{static_cast<float>(props.grid.width() -
                                         props.layout.footerCounterWidth -
                                         props.layout.footerGap)};
    return rect(props.transform.position.x, y, width,
                static_cast<float>(props.layout.footerHeight));
  }

  static RectTransform flagCounterTransform(const MinesweeperUIProps &props) {
    const float y{props.transform.position.y + props.grid.height() +
                  props.layout.padding};
    return rect(props.transform.position.x + props.grid.width() -
                    props.layout.footerCounterWidth,
                y, static_cast<float>(props.layout.footerCounterWidth),
                static_cast<float>(props.layout.footerHeight));
  }

  static RectTransform centeredTransform(const MinesweeperUIProps &props) {
    const Vec2i content{props.grid.width(), props.grid.height() +
                                                props.layout.padding +
                                                props.layout.footerHeight};
    const Vec2i viewport{hasArea(props.displayState.drawableSize)
                             ? props.displayState.drawableSize
                             : props.displayState.windowSize};
    const float x{
        static_cast<float>(std::max(0, viewport.x - content.x) * 0.5)};
    const float y{
        static_cast<float>(std::max(0, viewport.y - content.y) * 0.5)};
    return rect(x, y, static_cast<float>(content.x),
                static_cast<float>(content.y));
  }

  void relayout() {
    _props.transform = centeredTransform(_props);
    IDisplayObject::setTransform(_props.transform);
    _grid->setTransform(gridTransform(_props));
    _newGameButton.setTransform(newGameTransform(_props));
    _flagCounter.setTransform(flagCounterTransform(_props));
  }
};
