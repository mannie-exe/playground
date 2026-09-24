#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <minesweeper/ViewResources.hpp>
#include <ui/controls/Button.hpp>

struct NewGameButtonProps {
  playground::ui::ButtonProps button;
  playground::math::ColorRGBA8 labelColor{255, 255, 255, 255};
  std::string label{"New Game"};
};

class NewGameButton : public playground::ui::Button {
  std::function<void()> _request;

  std::optional<std::uint64_t> _secondaryPointer;

protected:
  void paint(playground::rendering::PaintContext &context) const override {
    if (_secondaryPointer && isEnabled())
      context.fill({{}, bounds().size}, props().pressed);
    else
      Button::paint(context);
  }

  void onDetach() noexcept override {
    _secondaryPointer.reset();
    Button::onDetach();
  }

  void onDefaultEvent(playground::ui::UIEvent &event) override {
    using namespace playground::ui;
    if (event.type == EventType::PointerCancel ||
        event.type == EventType::FocusLost ||
        (event.type == EventType::PointerUp &&
         _secondaryPointer == event.pointer &&
         (event.button == 1 || event.button == 3))) {
      _secondaryPointer.reset();
      releaseAllPointers();
      invalidatePaint();
    }
    // Secondary presses show feedback; only primary mouse-down requests a new
    // game.
    if (event.type == EventType::PointerDown && event.button == 3 &&
        !event.handled && isEnabled()) {
      _secondaryPointer = event.pointer;
      capturePointer(event.pointer);
      invalidatePaint();
      event.handled = true;
      return;
    }
    if (event.type == EventType::PointerDown && event.button == 1 &&
        !event.handled && isEnabled()) {
      event.handled = true;
      _request();
      return;
    }
    if (event.type == EventType::KeyDown || event.type == EventType::KeyUp)
      return;
    Button::onDefaultEvent(event);
  }

public:
  NewGameButton(const playground::minesweeper::ViewResources &resources,
                playground::math::Size2 size, const NewGameButtonProps &props,
                std::function<void()> request)
      : Button{playground::minesweeper::makeLabel(resources, props.label,
                                                  props.labelColor, size),
               props.button},
        _request{std::move(request)} {
    if (!_request)
      throw std::invalid_argument("New Game requires a reset request callback");
    setContentAlignment(playground::layout::Alignment::stretch());
    setFocusable(false);
    auto semantics = semanticProps();
    semantics.name = props.label;
    setSemanticProps(std::move(semantics));
  }
};
