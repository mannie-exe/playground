#pragma once

#include <cmath>
#include <string>

#include <SDL3/SDL_surface.h>

#include <study_sdl3/interfaces/IInteractable.hpp>
#include <study_sdl3/minesweeper/Config.hpp>
#include <study_sdl3/minesweeper/MinesweeperEvents.hpp>
#include <study_sdl3/support/SDLPrimitives.hpp>
#include <study_sdl3/ui/Button.hpp>
#include <study_sdl3/ui/DisplayImage.hpp>
#include <study_sdl3/ui/DisplayText.hpp>

class MinesweeperCell : public Button {
  study_sdl3::minesweeper::MinesweeperEvents _events;

  Vec2i _gridPos;
  int _adjacentBombs;
  bool _bomb;
  bool _cleared;
  Font &_font;

  bool _dirty{false};

public:
  MinesweeperCell(const RectTransform transform, const bool bomb,
                  const Vec2i gridPos, IInteractable &parent,
                  const study_sdl3::minesweeper::MinesweeperEvents &events,
                  const std::string &bombImagePath, Font &font,
                  const bool cleared = false, const int adjacentBombs = 0)
      : Button{
            transform,
            parent,
            {.baseColor = study_sdl3::minesweeper::config::buttonBaseColor,
             .hoverColor = study_sdl3::minesweeper::config::buttonHoverColor,
             .activeColor = study_sdl3::minesweeper::config::buttonActiveColor},
            bomb ? ButtonContent{.icon = std::make_unique<DisplayImage>(
                                     bombImagePath,
                                     rect(transform.position.x +
                                              study_sdl3::minesweeper::config::
                                                  bombImagePadding,
                                          transform.position.y +
                                              study_sdl3::minesweeper::config::
                                                  bombImagePadding,
                                          transform.size.x -
                                              study_sdl3::minesweeper::config::
                                                      bombImagePadding *
                                                  2,
                                          transform.size.y -
                                              study_sdl3::minesweeper::config::
                                                      bombImagePadding *
                                                  2),
                                     SurfaceRenderProps{}, false)}
                 : ButtonContent{}},
        _events{events}, _gridPos{gridPos}, _bomb{bomb}, _cleared{cleared},
        _font{font}, _adjacentBombs{adjacentBombs} {}
  ~MinesweeperCell() = default;

  int getAdjacentBombs() const { return _adjacentBombs; }
  bool isBomb() const { return _bomb; }
  bool isCleared() const { return _cleared; }

  void incrementAdjacentBombs() {
    if (isBomb())
      return;
    _adjacentBombs++;
    _dirty = true;
  }

  void clearCell(const bool propagate = true) {
    ButtonStyle style{getStyle()};

    if (isBomb()) {
      showIcon();
      style.baseColor = style.hoverColor =
          study_sdl3::minesweeper::config::bombBackgroundColor;
    } else {
      style.baseColor = style.hoverColor =
          study_sdl3::minesweeper::config::buttonClearedColor;
    }

    if (_dirty) {
      const std::string value{std::format("{}", getAdjacentBombs())};

      if (!Button::getLabel()) {
        TextProps textProps{.value = value,
                            .style = {.fgColor =
                                          study_sdl3::minesweeper::config::
                                              labelColors[getAdjacentBombs()]}};
        DisplayTextProps displayTextProps{
            .wrapToTransform = false,
            .alignment = {.horizontal = HorizontalAlign::Center,
                          .vertical = VerticalAlign::Middle}};

        Button::setLabel(
            DisplayText{textProps,
                        _font.cloneWith(FontPropsPatch{.size = 72.0f}),
                        displayTextProps,
                        {},
                        _transform});
      } else {
        Button::getLabel()->setValue(value);
      }
    }

    setStyle(style);
    setCleared();

    if (isBomb())
      publishEventParent({.user = {.type = _events.bombDetonated,
                                   .data1 = &_gridPos,
                                   .data2 = this}});
    else if (propagate && !getAdjacentBombs())
      publishEventParent({.user = {.type = _events.cellCleared,
                                   .data1 = &_gridPos,
                                   .data2 = this}});
  }

  EventResult handleEvent(const SDL_Event &event) override {
    EventResult result{Button::handleEvent(event)};

    if (event.type == _events.cellCleared) {
      const Vec2i &targetPos{*static_cast<const Vec2i *>(event.user.data1)};

      if (!isAdjacent(targetPos) || isCleared() || isBomb())
        return result;

      clearCell();
      return combine(result, EventResult::Handled);
    }
    return result;
  }

  EventResult onMouseClick(const SDL_MouseButtonEvent &) override {
    publishEventParent(
        {.user = {.type = _events.cellHit, .data1 = &_gridPos, .data2 = this}});

    if (isCleared())
      return EventResult::Consumed;

    clearCell();
    return EventResult::Consumed;
  }

  void render(SDL_Surface &targetSurface) override {
    Button::render(targetSurface);
  }

  bool isAdjacent(Vec2i gridPos) const {
    return std::abs(gridPos.x - _gridPos.x) <= 1 &&
           std::abs(gridPos.y - _gridPos.y) <= 1;
  }

  MinesweeperCell(MinesweeperCell &&) noexcept = default;

private:
  void setBomb(bool bomb = true) { _bomb = bomb; }
  void setCleared(const bool cleared = true) { _cleared = cleared; }
};
