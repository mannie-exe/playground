#include <string>

#include <minesweeper/MinesweeperCell.hpp>

void MinesweeperCell::onDefaultEvent(playground::ui::UIEvent &event) {
  using namespace playground::ui;
  if (event.type == EventType::PointerDown && !event.handled && isEnabled() &&
      (event.button == 1 || event.button == 3)) {
    event.handled = true;
    if (event.button == 3)
      performAction(CustomAction{1}, ActionSource::Pointer);
    else
      performAction(Activate{}, ActionSource::Pointer);
    return;
  }
  if (event.type == EventType::KeyDown && !event.handled && hasFocus() &&
      isEnabled() &&
      (event.logicalKey == Key::Space || event.logicalKey == Key::Enter)) {
    event.handled = true;
    if (!event.repeat)
      if (event.shift)
        performAction(CustomAction{1}, event.source);
      else
        performAction(Activate{}, event.source);
    return;
  }
  if (event.type == EventType::KeyUp)
    return;
  Button::onDefaultEvent(event);
}

playground::ui::SemanticState MinesweeperCell::semanticState() const {
  auto result = Button::semanticState();
  if (isEnabled() && !isCleared())
    result.customActions.push_back(
        {1, isFlagged() ? "Remove flag" : "Place flag"});
  return result;
}

playground::ui::ActionResult
MinesweeperCell::performAction(const playground::ui::UIAction &action,
                               playground::ui::ActionSource source) {
  using namespace playground::ui;
  if (!isEnabled())
    return ActionResult::Unavailable;
  if (std::holds_alternative<Activate>(action)) {
    _activate(_gridPos, 1);
    return ActionResult::Applied;
  }
  if (const auto *custom = std::get_if<CustomAction>(&action);
      custom && custom->id == 1) {
    if (isCleared())
      return ActionResult::Unavailable;
    _activate(_gridPos, 3);
    return ActionResult::Applied;
  }
  return Button::performAction(action, source);
}

MinesweeperCell::MinesweeperCell(
    float cellSize, const MinesweeperModel &model,
    playground::math::Vec2i position, const MinesweeperCellStyle &style,
    const playground::minesweeper::ViewResources &resources,
    std::function<void(playground::math::Vec2i, int)> activate)
    : Button{nullptr, style.button}, _model{model}, _gridPos{position},
      _style{style}, _resources{resources}, _labelSize{cellSize, cellSize},
      _activate{std::move(activate)} {
  using namespace playground;
  if (!_activate || style.labelColors.size() < 9 || style.iconPadding < 0)
    throw std::invalid_argument("Invalid Minesweeper cell configuration");
  auto layers = std::make_unique<ui::ZStack>(
      ui::ZStackProps{layout::Alignment::stretch()});
  auto label =
      minesweeper::makeLabel(resources, "", style.labelColors[0], _labelSize);
  _label = label.get();
  layers->append(std::move(label));
  auto icon = [&](bool bomb) {
    auto box = std::make_unique<ui::Box>(
        layout::BoxProps{.padding = math::Insets::all(
                             static_cast<float>(style.iconPadding))},
        ui::BoxContentProps{layout::Alignment::stretch()});
    box->setChild(minesweeper::makeIcon(
        resources, bomb,
        bomb ? math::ColorRGBA8{255, 255, 255, 255} : style.flagColor));
    auto *pointer = box.get();
    layers->append(std::move(box));
    return pointer;
  };
  // A new board can change this cell's bomb state without replacing its view.
  _bombIcon = icon(true);
  _flagIcon = icon(false);
  setChild(std::move(layers));
  setContentAlignment(layout::Alignment::stretch());
  setFocusable(false);
  auto semantics = semanticProps();
  semantics.name = "Row " + std::to_string(position.y + 1) + ", column " +
                   std::to_string(position.x + 1);
  semantics.description =
      "Arrows move; Enter/Space reveal; Shift+Enter/Space flag";
  setSemanticProps(std::move(semantics));
  synchronize();
}

void MinesweeperCell::synchronize() {
  if (_revision == _model.revision())
    return;
  using playground::ui::Visibility;
  const auto &cell = state();
  auto button = _style.button;
  if (cell.cleared)
    button.normal = button.hover =
        cell.bomb ? (cell.revealed ? _style.revealedColor : _style.bombColor)
                  : _style.clearedColor;
  const std::string value = cell.cleared && !cell.bomb && cell.adjacentBombs
                                ? std::to_string(cell.adjacentBombs)
                                : "";
  if (_label->props().value != value) {
    auto text = _label->props();
    text.value = value;
    text.font =
        playground::minesweeper::fittedFont(_resources, value, _labelSize);
    text.foreground = _style.labelColors.at(cell.adjacentBombs);
    _label->setProps(std::move(text));
  }
  setButtonProps(button);
  _bombIcon->setVisibility(cell.cleared && cell.bomb ? Visibility::Visible
                                                     : Visibility::Hidden);
  _flagIcon->setVisibility(cell.flagged ? Visibility::Visible
                                        : Visibility::Hidden);
  auto semantics = semanticProps();
  semantics.value = cell.flagged    ? "Flagged"
                    : !cell.cleared ? "Covered"
                    : cell.bomb     ? "Bomb"
                    : cell.adjacentBombs
                        ? std::to_string(cell.adjacentBombs) + " adjacent bombs"
                        : "Empty";
  setSemanticProps(std::move(semantics));
  _revision = _model.revision();
}
