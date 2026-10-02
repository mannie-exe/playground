#include <format>
#include <memory>
#include <utility>

#include <menu/Config.hpp>
#include <menu/MenuUI.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Button.hpp>

namespace playground::menu {
MenuUI::MenuUI(AssetRegistry &assets, FontHandle font, FontHandle titleFont,
               std::function<void(AppId)> launch)
    : Box{{.padding = math::Insets::all(padding)},
          {.contentAlignment = layout::Alignment::center()}} {
  setPaintStyle({.themeBackground = true});
  auto column = std::make_unique<ui::VStack>(
      layout::StackProps{.gap = gap,
                         .childrenAlignment = layout::CrossAlignment::Stretch},
      layout::BoxProps{.width = layout::SizeRule::fixed(contentWidth)});
  column->append(std::make_unique<ui::Text>(
      assets, ui::TextProps{.value = "Me n' U",
                            .font = std::move(titleFont),
                            .paragraphAlignment = layout::Align::Center,
                            .contentAlignment = layout::Alignment::center(),
                            .textRole = ui::TextRole::Display}));
  for (const auto &entry : entries) {
    auto label = std::make_unique<ui::Text>(
        assets, ui::TextProps{.value = std::format("{}  {}", entry.shortcut,
                                                   toString(entry.app)),
                              .font = font,
                              .contentAlignment = layout::Alignment::center(),
                              .textRole = ui::TextRole::Label});
    auto button = std::make_unique<ui::Button>(
        std::move(label), ui::ButtonProps{},
        layout::BoxProps{.minHeight = buttonHeight});
    button->setControlLayout(ui::ControlLayout::Choice);
    button->setSemanticProps({.role = ui::SemanticRole::Button,
                              .name = std::string{toString(entry.app)}});
    _connections.push_back(
        button->onActivate([launch, app = entry.app] { launch(app); }));
    column->append(std::move(button));
  }
  column->append(std::make_unique<ui::Text>(
      assets,
      ui::TextProps{
          .value =
              "1-5: launch | Tab: focus | Enter: open\nEsc: settings | Q: quit",
          .font = font,
          .wrap = ui::TextWrap::AvailableInlineSize,
          .paragraphAlignment = layout::Align::Center,
          .textRole = ui::TextRole::Caption}));
  auto status = std::make_unique<ui::Text>(
      assets, ui::TextProps{.font = std::move(font),
                            .wrap = ui::TextWrap::AvailableInlineSize,
                            .textRole = ui::TextRole::Body});
  _status = status.get();
  column->append(std::move(status));
  setChild(std::move(column));
  setSemanticProps(
      {.role = ui::SemanticRole::Group, .name = "Application launcher"});
}

void MenuUI::setStatus(std::string value) {
  if (_status->props().value == value)
    return;
  _status->applyPatch({.value = Patch<std::string>::set(std::move(value))});
}
} // namespace playground::menu
