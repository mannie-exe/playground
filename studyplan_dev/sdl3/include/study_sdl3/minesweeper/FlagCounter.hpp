#pragma once

#include <format>
#include <optional>
#include <utility>

#include <study_sdl3/ui/Button.hpp>
#include <study_sdl3/ui/DisplayText.hpp>
#include <study_sdl3/ui/DisplayVector.hpp>

struct FlagCounterProps {
  ButtonStyle button;
  SDL_Color iconColor{255, 255, 255, 255};
  SDL_Color labelColor{255, 255, 255, 255};
  int amount{};
};

struct FlagCounterPropsPatch {
  std::optional<ButtonStyle> button;
  std::optional<SDL_Color> iconColor;
  std::optional<SDL_Color> labelColor;
  std::optional<int> amount;
};

class FlagCounter : public Button {
  FlagCounterProps _props;
  FontHandle _font;

public:
  FlagCounter(const RectTransform transform, IInteractable &parent,
              SurfaceHandle flagImage, FontHandle font,
              const FlagCounterProps props = {})
      : Button{
            transform,
            parent,
            props.button,
            {.label = std::make_unique<DisplayText>(
                 TextProps{.value = std::format("{}", props.amount),
                           .style = {.fgColor = props.labelColor}},
                 cloneFontToSize(
                     font, std::format("{}", props.amount),
                     Vec2f{transform.size.x * 0.5f, transform.size.y}, 0.825f),
                 DisplayTextProps{
                     .wrapToTransform = false,
                     .alignment = {.horizontal = HorizontalAlign::Center,
                                   .vertical = VerticalAlign::Middle}},
                 SurfaceRenderProps{},
                 rect(transform.position.x, transform.position.y,
                      transform.size.x / 2, transform.size.y)),
             .icon = std::make_unique<DisplayVector>(
                 flagImage,
                 rect(transform.position.x + transform.size.x / 2,
                      transform.position.y, transform.size.x / 2,
                      transform.size.y),
                 SurfaceRenderProps{
                     .blit = {.fitMode = SurfaceFitMode::Contain},
                     .appearance = {.colorMod = props.iconColor}})},
            InteractionState::Disabled},
        _props{props}, _font{std::move(font)} {}

  ~FlagCounter() = default;

  int getAmount() const { return _props.amount; }
  const FlagCounterProps &getProps() const { return _props; }

  void setAmount(const int amount) {
    if (_props.amount == amount)
      return;
    _props.amount = amount;
    if (Button::getLabel()) {
      Button::getLabel()->setValue(std::format("{}", amount));
      Button::getLabel()->replaceFont(cloneFontToSize(
          _font, std::format("{}", amount),
          Vec2f{_transform.size.x * 0.5f, _transform.size.y}, 0.825f));
    }
  }

  void applyPropsPatch(const FlagCounterPropsPatch &patch) {
    if (patch.button) {
      _props.button = *patch.button;
      Button::setStyle(_props.button);
    }
    if (patch.labelColor) {
      _props.labelColor = *patch.labelColor;
      if (Button::getLabel())
        Button::getLabel()->setStyle(
            TextStylePatch{.fgColor = _props.labelColor});
    }
    if (patch.amount)
      setAmount(*patch.amount);
    if (patch.iconColor)
      _props.iconColor = *patch.iconColor;
  }

  EventResult handleEvent(const SDL_Event &event) override {
    if (!isVisible())
      return EventResult::Ignored;
    return Button::handleEvent(event);
  }

  void render(SDL_Surface &targetSurface) override {
    if (!isVisible())
      return;
    Button::render(targetSurface);
  }

  FlagCounter(FlagCounter &&) noexcept = default;
};
