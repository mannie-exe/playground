#include <minesweeper/MinesweeperCell.hpp>

void MinesweeperCell::onDefaultEvent(playground::ui::UIEvent &event) {
  using namespace playground::ui;
  if (event.type == EventType::PointerDown && !event.handled && isEnabled() &&
      (event.button == 1 || event.button == 3)) {
    event.handled = true;
    _publish(
        {.user = {.type = _events.cellHit, .data1 = &_gridPos, .data2 = this}});
    if (_cleared)
      return;
    if (event.button == 1) {
      if (!_flagged)
        clearCell();
    } else {
      setFlagged(!_flagged);
      playground::minesweeper::publishFlagChange(
          _events, {_gridPos, _flagged, _gridGeneration});
    }
    return;
  }
  if (event.type == EventType::KeyDown || event.type == EventType::KeyUp)
    return;
  Button::onDefaultEvent(event);
}

MinesweeperCell::MinesweeperCell(
    float cellSize, bool bomb, playground::math::Vec2i position,
    const MinesweeperCellStyle &style,
    const playground::minesweeper::MinesweeperEvents &events,
    const playground::minesweeper::ViewResources &resources, Sint32 generation,
    std::function<EventResult(const SDL_Event &)> publish)
    : Button{nullptr, style.button}, _events{events},
      _gridGeneration{generation}, _publish{std::move(publish)},
      _gridPos{position}, _style{style}, _resources{resources},
      _labelSize{cellSize, cellSize}, _bomb{bomb} {
  using namespace playground;
  if (!_publish || style.labelColors.size() < 9 || style.iconPadding < 0)
    throw std::invalid_argument("Invalid Minesweeper cell configuration");

  auto layers = std::make_unique<ui::ZStack>(
      ui::ZStackProps{layout::Alignment::stretch()});
  auto label =
      minesweeper::makeLabel(resources, "", style.labelColors[0], _labelSize);
  _label = label.get();
  layers->append(std::move(label));
  auto icon = [&](bool isBomb) {
    auto box = std::make_unique<ui::Box>(
        layout::BoxProps{.padding = math::Insets::all(
                             static_cast<float>(style.iconPadding))},
        ui::BoxContentProps{layout::Alignment::stretch()});
    box->setChild(minesweeper::makeIcon(
        resources, isBomb,
        isBomb ? math::ColorRGBA8{255, 255, 255, 255} : style.flagColor));
    auto *pointer = box.get();
    layers->append(std::move(box));
    return pointer;
  };
  if (bomb)
    _bombIcon = icon(true);
  _flagIcon = icon(false);
  setChild(std::move(layers));
  setContentAlignment(layout::Alignment::stretch());
  setFocusable(false);
  auto semantics = semanticProps();
  semantics.name =
      "Cell " + std::to_string(position.x) + ", " + std::to_string(position.y);
  setSemanticProps(std::move(semantics));
  synchronizeIcons();
}

void MinesweeperCell::incrementAdjacentBombs() {
  if (!_bomb) {
    ++_adjacentBombs;
    _dirty = true;
  }
}

void MinesweeperCell::setFlagged(bool flagged) {
  if (_cleared || _flagged == flagged)
    return;
  _flagged = flagged;
  synchronizeIcons();
}

void MinesweeperCell::clearCell(bool propagate) {
  if (_cleared)
    return;
  auto style = Button::props();
  style.normal = style.hover =
      _bomb ? (_revealed ? _style.revealedColor : _style.bombColor)
            : _style.clearedColor;
  if (_dirty) {
    auto text = _label->props();
    text.value = std::to_string(_adjacentBombs);
    text.font =
        playground::minesweeper::fittedFont(_resources, text.value, _labelSize);
    text.foreground = _style.labelColors.at(_adjacentBombs);
    _label->setProps(std::move(text));
    _dirty = false;
  }
  if (_flagged) {
    _flagged = false;
    playground::minesweeper::publishFlagChange(
        _events, {_gridPos, false, _gridGeneration});
  }
  Button::setProps(style);
  _cleared = true;
  synchronizeIcons();
  if (propagate)
    _publish({.user = {.type = _bomb && !_revealed ? _events.bombDetonated
                                                   : _events.cellCleared,
                       .data1 = &_gridPos,
                       .data2 = this}});
}

EventResult MinesweeperCell::handleEvent(const SDL_Event &event) {
  if (event.type != _events.cellCleared)
    return EventResult::Ignored;
  const auto position =
      *static_cast<const playground::math::Vec2i *>(event.user.data1);
  if (!isAdjacent(position) || _cleared || _bomb)
    return EventResult::Ignored;
  clearCell();
  return EventResult::Handled;
}

void MinesweeperCell::synchronizeIcons() {
  using playground::ui::Visibility;
  if (_bombIcon)
    _bombIcon->setVisibility(_cleared && _bomb ? Visibility::Visible
                                               : Visibility::Hidden);
  _flagIcon->setVisibility(_flagged ? Visibility::Visible : Visibility::Hidden);
}
