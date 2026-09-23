#pragma once

#include <minesweeper/ViewResources.hpp>
#include <optional>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>

struct FlagCounterProps {
  playground::ui::ButtonProps button;
  playground::math::ColorRGBA8 iconColor{255, 255, 255, 255};
  playground::math::ColorRGBA8 labelColor{255, 255, 255, 255};
  int amount{};
};

struct FlagCounterPatch {
  std::optional<playground::ui::ButtonProps> button;
  std::optional<playground::math::ColorRGBA8> iconColor;
  std::optional<playground::math::ColorRGBA8> labelColor;
  std::optional<int> amount;
};

class FlagCounter : public playground::ui::Box {
  FlagCounterProps _props;
  playground::minesweeper::ViewResources _resources;
  playground::math::Size2 _labelSize;

  playground::ui::Text *_label{};
  playground::ui::Vector *_icon{};

public:
  FlagCounter(const playground::minesweeper::ViewResources &resources,
              playground::math::Size2 size, FlagCounterProps props = {})
      : _props{props}, _resources{resources},
        _labelSize{size.width / 2, size.height} {
    using namespace playground;
    auto row = std::make_unique<ui::HStack>(layout::StackProps{
        .childrenAlignment = layout::CrossAlignment::Stretch});
    auto label = minesweeper::makeLabel(resources, std::to_string(props.amount),
                                        props.labelColor, _labelSize);
    auto icon = minesweeper::makeIcon(resources, false, props.iconColor);
    _label = label.get();
    _icon = icon.get();
    label->setBoxProps({.width = layout::SizeRule::fixed(size.width / 2)});
    icon->setBoxProps({.width = layout::SizeRule::fixed(size.width / 2)});
    row->append(std::move(label));
    row->append(std::move(icon));
    setChild(std::move(row));
    setContentAlignment(layout::Alignment::stretch());
    setBackground(props.button.disabled);
    setSemanticProps({.role = ui::SemanticRole::Group,
                      .name = "Flag counter",
                      .enabled = false});
  }

  int getAmount() const { return _props.amount; }
  const FlagCounterProps &getProps() const { return _props; }

  void setAmount(int amount) {
    if (_props.amount == amount)
      return;
    auto text = _label->props();
    text.value = std::to_string(amount);
    text.font =
        playground::minesweeper::fittedFont(_resources, text.value, _labelSize);
    _label->setProps(std::move(text));
    _props.amount = amount;
  }

  void applyPropsPatch(const FlagCounterPatch &patch) {
    if (patch.amount)
      setAmount(*patch.amount);
    if (patch.button) {
      setBackground(patch.button->disabled);
      _props.button = *patch.button;
    }
    if (patch.labelColor) {
      auto text = _label->props();
      text.foreground = *patch.labelColor;
      _label->setProps(std::move(text));
      _props.labelColor = *patch.labelColor;
    }
    if (patch.iconColor) {
      auto icon = _icon->props();
      icon.content.paint.tint = *patch.iconColor;
      _icon->setProps(std::move(icon));
      _props.iconColor = *patch.iconColor;
    }
  }
};
