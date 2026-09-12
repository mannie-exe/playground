#pragma once

#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_surface.h>

#include <interfaces/IInteractable.hpp>
#include <minesweeper/Config.hpp>
#include <minesweeper/MinesweeperEvents.hpp>
#include <support/SDLPrimitives.hpp>
#include <surface/DrawableSurface.hpp>
#include <ui/Button.hpp>
#include <ui/DisplayText.hpp>
#include <ui/DisplayVector.hpp>

struct MinesweeperCellStyle {
  SDL_Color bombColor;
  SDL_Color revealedColor;
  SDL_Color clearedColor;

  SDL_Color flagColor{255, 255, 255, 255};

  int iconPadding{16};

  std::vector<SDL_Color> labelColors;

  ButtonStyle button;
};

class MinesweeperCell : public Button {
  playground::minesweeper::MinesweeperEvents _events;

  Vec2i _gridPos;
  MinesweeperCellStyle _style;
  int _adjacentBombs;
  bool _bomb;
  bool _flagged;
  bool _cleared;
  bool _revealed;
  SurfaceHandle _bombImage;
  SurfaceHandle _flagImage;
  FontHandle _font;
  std::unique_ptr<DisplayVector> _bombIcon;
  std::unique_ptr<DisplayVector> _flagIcon;

  bool _dirty{false};

public:
  MinesweeperCell(const RectTransform transform, const bool bomb,
                  const bool flagged, const Vec2i gridPos,
                  IInteractable &parent, const MinesweeperCellStyle style,
                  const playground::minesweeper::MinesweeperEvents &events,
                  SurfaceHandle bombImage, SurfaceHandle flagImage,
                  FontHandle font, const bool cleared = false,
                  const bool revealed = false, const int adjacentBombs = 0)
      : Button{transform, parent, style.button, ButtonContent{}},
        _events{events}, _gridPos{gridPos}, _style{style},
        _adjacentBombs{adjacentBombs}, _bomb{bomb}, _flagged{flagged},
        _cleared{cleared}, _revealed{revealed},
        _bombImage{std::move(bombImage)}, _flagImage{std::move(flagImage)},
        _font{std::move(font)},
        _bombIcon{bomb ? std::make_unique<DisplayVector>(
                             _bombImage, iconTransform(transform, style))
                       : nullptr},
        _flagIcon{flagged
                      ? std::make_unique<DisplayVector>(
                            _flagImage, iconTransform(transform, style),
                            SurfaceRenderProps{
                                .appearance = {.colorMod = style.flagColor}})
                      : nullptr} {}
  ~MinesweeperCell() = default;

  int getAdjacentBombs() const { return _adjacentBombs; }
  bool isBomb() const { return _bomb; }
  bool isFlagged() const { return _flagged; }
  bool isCleared() const { return _cleared; }
  bool isRevealed() const { return _revealed; }

  void incrementAdjacentBombs() {
    if (isBomb())
      return;
    _adjacentBombs++;
    _dirty = true;
  }

  void setFlagged(const bool flagged = true) {
    if (isCleared() || _flagged == flagged)
      return;

    if (flagged && !_flagIcon)
      _flagIcon = std::make_unique<DisplayVector>(
          _flagImage, iconTransform(_transform, _style),
          SurfaceRenderProps{.appearance = {.colorMod = _style.flagColor}});

    _flagged = flagged;
  }

  void setRevealed(const bool revealed = true) { _revealed = revealed; }

  bool isAdjacent(Vec2i gridPos) const {
    return std::abs(gridPos.x - _gridPos.x) <= 1 &&
           std::abs(gridPos.y - _gridPos.y) <= 1 && gridPos != _gridPos;
  }

  void clearCell(const bool propagate = true) {
    if (isCleared())
      return;

    ButtonStyle style{Button::getStyle()};

    if (isBomb()) {
      Button::showIcon();
      if (isRevealed()) {
        style.baseColor = style.hoverColor = _style.revealedColor;
      } else {
        style.baseColor = style.hoverColor = _style.bombColor;
      }
    } else {
      style.baseColor = style.hoverColor = _style.clearedColor;
    }

    if (_dirty) {
      const std::string value{std::format("{}", getAdjacentBombs())};

      if (!Button::getLabel()) {
        TextProps textProps{
            .value = value,
            .style = {.fgColor = _style.labelColors[getAdjacentBombs()]}};
        DisplayTextProps displayTextProps{
            .wrapToTransform = false,
            .alignment = {.horizontal = HorizontalAlign::Center,
                          .vertical = VerticalAlign::Middle}};

        Button::setLabel(
            DisplayText{textProps,
                        cloneFontToSize(_font, value, _transform.size, 0.825f),
                        displayTextProps,
                        {},
                        _transform});
      } else {
        Button::getLabel()->setValue(value);
      }
    }

    if (_flagged) {
      _flagged = false;
      publishEventGlobal({.user = {.type = _events.flagToggled,
                                   .data1 = &_gridPos,
                                   .data2 = &_flagged}});
    }

    setStyle(style);
    setCleared();

    if (propagate && isBomb() && !isRevealed())
      publishEventParent({.user = {.type = _events.bombDetonated,
                                   .data1 = &_gridPos,
                                   .data2 = this}});
    else if (propagate)
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

  EventResult
  onMouseLeftClick(const SDL_MouseButtonEvent &mButtonEvent) override {
    publishEventParent(
        {.user = {.type = _events.cellHit, .data1 = &_gridPos, .data2 = this}});

    if (isCleared() || isFlagged())
      return EventResult::Consumed;

    clearCell();
    return EventResult::Consumed;
  }

  EventResult
  onMouseRightClick(const SDL_MouseButtonEvent &mButtonEvent) override {
    publishEventParent(
        {.user = {.type = _events.cellHit, .data1 = &_gridPos, .data2 = this}});

    if (isCleared())
      return EventResult::Consumed;

    setFlagged(!isFlagged());
    publishEventGlobal({.user = {.type = _events.flagToggled,
                                 .data1 = &_gridPos,
                                 .data2 = &_flagged}});
    return EventResult::Consumed;
  }

  void render(SDL_Surface &targetSurface) override {
    Button::render(targetSurface);
    if (_flagged && _flagIcon)
      _flagIcon->render(targetSurface);
    else if (_cleared && _bomb && _bombIcon)
      _bombIcon->render(targetSurface);
  }

  MinesweeperCell(MinesweeperCell &&) noexcept = default;

private:
  static RectTransform iconTransform(const RectTransform transform,
                                     const MinesweeperCellStyle &style) {
    const float padding{static_cast<float>(style.iconPadding)};
    return rect(transform.position.x + padding, transform.position.y + padding,
                transform.size.x - padding * 2.0f,
                transform.size.y - padding * 2.0f);
  }

  void setBomb(bool bomb = true) { _bomb = bomb; }
  void setCleared(const bool cleared = true) { _cleared = cleared; }
};
