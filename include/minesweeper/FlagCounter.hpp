#pragma once

#include <optional>

#include <minesweeper/ViewResources.hpp>
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
                      .value = std::to_string(props.amount)});
  }

  int amount() const { return _props.amount; }

  const FlagCounterProps &props() const { return _props; }

  void setAmount(int amount) {
    if (_props.amount == amount)
      return;
    auto props = _props;
    props.amount = amount;
    setProps(props);
  }

  // Stage copies and font acquisition first. Child setters provide their own
  // guarantees; this is not rollback of arbitrary notification side effects.
  void setProps(FlagCounterProps props) {
    auto semantics = semanticProps();
    semantics.value = std::to_string(props.amount);
    auto text = _label->props();
    text.foreground = props.labelColor;
    if (_props.amount != props.amount) {
      text.value = std::to_string(props.amount);
      text.font = playground::minesweeper::fittedFont(_resources, text.value,
                                                      _labelSize);
    }
    auto icon = _icon->props();
    icon.content.paint.tint = props.iconColor;
    _label->setProps(std::move(text));
    _props.amount = props.amount;
    _props.labelColor = props.labelColor;
    _icon->setProps(std::move(icon));
    _props.iconColor = props.iconColor;
    setBackground(props.button.disabled);
    _props.button = props.button;
    setSemanticProps(std::move(semantics));
  }

  void applyPatch(const FlagCounterPatch &patch) {
    setProps({patch.button.value_or(_props.button),
              patch.iconColor.value_or(_props.iconColor),
              patch.labelColor.value_or(_props.labelColor),
              patch.amount.value_or(_props.amount)});
  }
};
