#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

#include <interfaces/IDisplayObject.hpp>
#include <interfaces/IInteractable.hpp>
#include <ui/DisplayText.hpp>
#include <ui/EventTypes.hpp>
#include <ui/InteractionState.hpp>
#include <ui/Rectangle.hpp>

struct ButtonStyle {
  SDL_Color baseColor{55, 82, 61, 255};
  SDL_Color hoverColor{131, 183, 141, 255};
  SDL_Color activeColor{117, 133, 120, 255};
  SDL_Color disabledColor{96, 96, 96, 255};
};

struct ButtonStylePatch {
  std::optional<SDL_Color> baseColor;
  std::optional<SDL_Color> hoverColor;
  std::optional<SDL_Color> activeColor;
  std::optional<SDL_Color> disabledColor;
};

struct ButtonContent {
  std::unique_ptr<DisplayText> label;
  std::unique_ptr<IDisplayObject> icon;
  // TODO: positioning/alignment, sizing, and rendering options
};

class Button : public IDisplayObject {
protected:
  Rectangle _background;
  IInteractable &_parent;
  ButtonStyle _style;
  InteractionState _state{InteractionState::Normal};
  ButtonContent _content;

public:
  Button(const RectTransform transform, IInteractable &parent,
         const ButtonStyle style = {}, ButtonContent content = {},
         const InteractionState state = InteractionState::Normal)
      : IDisplayObject{transform}, _background{transform}, _parent{parent},
        _style{style}, _state{state}, _content{std::move(content)} {}

  ButtonStyle getStyle() const { return _style; }
  InteractionState getState() const { return _state; }
  bool isDisabled() const { return _state == InteractionState::Disabled; }
  IDisplayObject *getIcon() { return _content.icon.get(); }
  const IDisplayObject *getIcon() const { return _content.icon.get(); }
  DisplayText *getLabel() { return _content.label.get(); }
  const DisplayText *getLabel() const { return _content.label.get(); }

  void setStyle(const ButtonStyle &style) { _style = style; }

  void applyStylePatch(const ButtonStylePatch &patch) {
    if (patch.baseColor)
      _style.baseColor = *patch.baseColor;
    if (patch.hoverColor)
      _style.hoverColor = *patch.hoverColor;
    if (patch.activeColor)
      _style.activeColor = *patch.activeColor;
    if (patch.disabledColor)
      _style.disabledColor = *patch.disabledColor;
  }
  void setDisabled(bool disabled = true) {
    _state = disabled ? InteractionState::Disabled : InteractionState::Normal;
  }
  void setEnabled(bool enabled = true) { setDisabled(!enabled); }
  void setIcon(std::unique_ptr<IDisplayObject> icon) {
    _content.icon = std::move(icon);
  }
  void setLabel(DisplayText label) {
    _content.label = std::make_unique<DisplayText>(std::move(label));
  }

  virtual EventResult handleEvent(const SDL_Event &event) override {
    if (!isVisible() || _state == InteractionState::Disabled)
      return EventResult::Ignored;

    if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_KEY_DOWN) {
      return onKey(event.key);
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION) {
      bool inObject = isPointInObject(event.motion.x, event.motion.y);
      if ((_state == InteractionState::Hovered ||
           _state == InteractionState::Pressed) &&
          !inObject)
        return onMouseExit(event.motion.x, event.motion.y);
      if ((_state == InteractionState::Normal ||
           _state == InteractionState::PressedOutside) &&
          inObject)
        return onMouseEnter(event.motion.x, event.motion.y);
      return onMouseMove(event.motion);
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      bool inObject = isPointInObject(event.button.x, event.button.y);
      if (event.button.button == SDL_BUTTON_LEFT && inObject)
        return onMouseLeftClick(event.button);
      if (event.button.button == SDL_BUTTON_RIGHT && inObject)
        return onMouseRightClick(event.button);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      bool inObject = isPointInObject(event.button.x, event.button.y);
      if ((_state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside) &&
          (event.button.button == SDL_BUTTON_LEFT ||
           event.button.button == SDL_BUTTON_RIGHT))
        return onMouseRelease(event.button, inObject);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      if (isPointInObject(event.wheel.mouse_x, event.wheel.mouse_y))
        return onMouseWheel(event.wheel);
      return EventResult::Ignored;
    }

    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
        event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE ||
        event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
        event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) {
      float mX, mY;
      SDL_GetMouseState(&mX, &mY);
      bool inObject = isPointInObject(mX, mY);

      if ((event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
           event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) &&
          (_state == InteractionState::Hovered ||
           _state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside)) {
        return resetInteraction();
      }

      if ((event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
           event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) &&
          inObject) {
        return onMouseEnter(mX, mY);
      }
    }

    return EventResult::Ignored;
  }

  void showIcon(const bool show = true) {
    if (!_content.icon)
      return;
    _content.icon->show(show);
  }

  void showLabel(const bool show = true) {
    if (!_content.label)
      return;
    _content.label->show(show);
  }

  void hideIcon(const bool hide = true) { showIcon(!hide); }
  void hideLabel(const bool hide = true) { showLabel(!hide); }

  virtual void publishEventParent(const SDL_Event &event) {
    _parent.handleEvent(event);
  }

  virtual void publishEventGlobal(const SDL_Event &event) {
    SDL_Event eventCopy{event};
    SDL_PushEvent(&eventCopy);
  }

  void renderWithStyle(SDL_Surface &targetSurface, const ButtonStyle &style) {
    if (!isVisible())
      return;
    const SDL_Color &color =
        _state == InteractionState::Disabled ? style.disabledColor
        : (_state == InteractionState::Pressed ||
           _state == InteractionState::PressedOutside)
            ? style.activeColor
        : _state == InteractionState::Hovered ? style.hoverColor
                                              : style.baseColor;
    _background.render(targetSurface, color);
    if (_content.icon)
      _content.icon->render(targetSurface);
    if (_content.label)
      _content.label->render(targetSurface);
  }

  virtual void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    renderWithStyle(targetSurface, _style);
  }

  virtual EventResult onKey(const SDL_KeyboardEvent &) {
    return EventResult::Ignored;
  }

  virtual EventResult onMouseMove(const SDL_MouseMotionEvent &) {
    return EventResult::Ignored;
  }

  virtual EventResult onMouseEnter(const float, const float) {
    if (_state == InteractionState::PressedOutside)
      _state = InteractionState::Pressed;
    else
      _state = InteractionState::Hovered;
    return EventResult::Handled;
  }

  virtual EventResult onMouseExit(const float, const float) {
    if (_state == InteractionState::Pressed)
      _state = InteractionState::PressedOutside;
    else
      _state = InteractionState::Normal;
    return EventResult::Handled;
  }

  virtual EventResult
  onMouseLeftClick(const SDL_MouseButtonEvent &mButtonEvent) {
    _state = InteractionState::Pressed;
    return EventResult::Consumed;
  }

  virtual EventResult
  onMouseRightClick(const SDL_MouseButtonEvent &mButtonEvent) {
    _state = InteractionState::Pressed;
    return EventResult::Consumed;
  }

  virtual EventResult onMouseRelease(const SDL_MouseButtonEvent &,
                                     const bool inObject) {
    _state = inObject ? InteractionState::Hovered : InteractionState::Normal;
    return inObject ? EventResult::Consumed : EventResult::Handled;
  }

  virtual EventResult onMouseWheel(const SDL_MouseWheelEvent &) {
    return EventResult::Ignored;
  }

  EventResult resetInteraction() {
    _state = InteractionState::Normal;
    return EventResult::Handled;
  }

  static FontHandle cloneFontToSize(const FontHandle &font,
                                    std::string_view value, Vec2f targetSize,
                                    float scale) {
    return cloneFontToSize(font, value, targetSize, Vec2f{scale, scale});
  }

  static FontHandle cloneFontToSize(const FontHandle &font,
                                    std::string_view value, Vec2f targetSize,
                                    Vec2f scale) {
    if (value.empty() || !hasArea(targetSize) || !hasArea(scale))
      return font;

    int measuredWidth{};
    int measuredHeight{};
    if (!TTF_GetStringSize(font->get(), value.data(), value.size(),
                           &measuredWidth, &measuredHeight) ||
        measuredWidth <= 0 || measuredHeight <= 0) {
      return font;
    }

    const float widthScale{targetSize.x * scale.x /
                           static_cast<float>(measuredWidth)};
    const float heightScale{targetSize.y * scale.y /
                            static_cast<float>(measuredHeight)};
    const float size{
        std::max(1.0f, font->getSize() * std::min(widthScale, heightScale))};
    return std::make_shared<Font>(
        font->cloneWith(FontPropsPatch{.size = size}));
  }
};
