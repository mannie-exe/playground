#pragma once

#include <SDL3/SDL_events.h>

#include <string>

#include <minesweeper/MinesweeperEvents.hpp>
#include <support/Font.hpp>
#include <ui/Button.hpp>
#include <ui/DisplayText.hpp>

struct NewGameButtonProps {
  ButtonStyle button;
  SDL_Color labelColor{255, 255, 255, 255};
  std::string label{"New Game"};
};

class NewGameButton : public Button {
  playground::minesweeper::MinesweeperEvents _events;

public:
  NewGameButton(const RectTransform transform, IInteractable &parent,
                const playground::minesweeper::MinesweeperEvents &events,
                FontHandle font, const NewGameButtonProps props = {})
      : Button{
            transform,
            parent,
            props.button,
            {.label = std::make_unique<DisplayText>(
                 TextProps{.value = props.label,
                           .style = {.fgColor = props.labelColor}},
                 cloneFontToSize(font, props.label, transform.size, 0.825f),
                 DisplayTextProps{
                     .wrapToTransform = false,
                     .alignment = {.horizontal = HorizontalAlign::Center,
                                   .vertical = VerticalAlign::Middle}},
                 SurfaceRenderProps{}, transform)}},
        _events{events} {}

  EventResult onMouseLeftClick(const SDL_MouseButtonEvent &) override {
    publishEventParent({.user = {.type = _events.newGameRequested}});
    return EventResult::Consumed;
  }

  NewGameButton(NewGameButton &&) noexcept = default;
};
