#include <ui/collections/VirtualGrid.hpp>

namespace playground::ui {

void VirtualGrid::rebuildIndex() {
  std::unordered_map<ItemKey, std::size_t> indices;
  for (std::size_t i = 0; i < _keys.size(); ++i)
    indices.emplace(_keys[i], i);
  const math::Size2 extent =
      _keys.empty()
          ? math::Size2{}
          : math::Size2{layout::detail::checked(_props.columns * strideX() -
                                                _props.gap.horizontal),
                        layout::detail::checked(rows() * strideY() -
                                                _props.gap.vertical)};
  _indices = std::move(indices);
  _extent = extent;
}

void VirtualGrid::clampOffset() noexcept {
  _offset.x = std::clamp(_offset.x, 0.0f,
                         std::max(0.0f, _extent.width - _viewport.width));
  _offset.y = std::clamp(_offset.y, 0.0f,
                         std::max(0.0f, _extent.height - _viewport.height));
}

std::optional<VirtualGrid::Anchor> VirtualGrid::anchor() const {
  if (_keys.empty())
    return {};
  const auto col = static_cast<std::size_t>(
      std::min(std::floor(_offset.x / strideX()),
               static_cast<double>(_props.columns - 1)));
  const auto row = static_cast<std::size_t>(std::min(
      std::floor(_offset.y / strideY()), static_cast<double>(rows() - 1)));
  const auto index = std::min(row * _props.columns + col, _keys.size() - 1);
  return Anchor{
      _keys[index],
      {static_cast<float>(_offset.x - (index % _props.columns) * strideX()),
       static_cast<float>(_offset.y - (index / _props.columns) * strideY())}};
}

void VirtualGrid::restoreAnchor(const std::optional<Anchor> &saved) {
  if (saved)
    if (auto it = _indices.find(saved->key); it != _indices.end()) {
      _offset = {
          layout::detail::checked((it->second % _props.columns) * strideX()) +
              saved->within.x,
          layout::detail::checked((it->second / _props.columns) * strideY()) +
              saved->within.y};
    }
  clampOffset();
}

void VirtualGrid::prepareChildren(MeasureContext &context,
                                  const layout::SizeConstraints &offered) {
  if (!offered.width.maximum || !offered.height.maximum)
    throw std::invalid_argument(
        "VirtualGrid requires a finite viewport on both axes");
  _viewport = {*offered.width.maximum, *offered.height.maximum};
  _direction = context.direction;
  clampOffset();
  _visible.clear();
  if (!_keys.empty() && math::hasArea(_viewport)) {
    const auto firstCol = static_cast<std::size_t>(
        std::min(static_cast<double>(_props.columns),
                 std::floor(std::max(0.0, static_cast<double>(_offset.x) -
                                              _props.overscan) /
                            strideX())));
    const auto firstRow = static_cast<std::size_t>(
        std::min(static_cast<double>(rows()),
                 std::floor(std::max(0.0, static_cast<double>(_offset.y) -
                                              _props.overscan) /
                            strideY())));
    const auto endCol = static_cast<std::size_t>(
        std::min(static_cast<double>(_props.columns),
                 std::ceil((static_cast<double>(_offset.x) + _viewport.width +
                            _props.overscan) /
                           strideX())));
    const auto endRow = static_cast<std::size_t>(
        std::min(static_cast<double>(rows()),
                 std::ceil((static_cast<double>(_offset.y) + _viewport.height +
                            _props.overscan) /
                           strideY())));
    for (auto row = firstRow; row < endRow; ++row)
      for (auto col = firstCol; col < endCol; ++col) {
        const auto index = row * _props.columns + col;
        if (index < _keys.size())
          _visible.push_back(_keys[index]);
      }
  }
  auto wanted = _visible;
  pinActiveItems(wanted);
  reconcile(std::move(wanted));
  for (const auto &child : children())
    child->measure(context, layout::SizeConstraints::tight(_props.cellExtent));
}

void VirtualGrid::arrangeChildren(ArrangeContext &context, math::Rect content) {
  _viewport = content.size;
  clampOffset();
  for (const auto &key : realizedKeys()) {
    const auto index = _indices.at(key);
    const float start =
        layout::detail::checked((index % _props.columns) * strideX()) -
        _offset.x;
    const float x = context.direction == layout::LayoutDirection::RightToLeft
                        ? content.right() - start - _props.cellExtent.width
                        : content.x() + start;
    realized(key)->arrange(
        context,
        {{x, content.y() +
                 layout::detail::checked((index / _props.columns) * strideY()) -
                 _offset.y},
         _props.cellExtent});
  }
}

void VirtualGrid::onDefaultEvent(UIEvent &event) {
  if (event.type != EventType::Wheel || event.phase == EventPhase::Capture ||
      event.defaultPrevented || _props.wheelStep == 0)
    return;
  const auto previous = _offset;
  const math::Vec2f direction{
      _direction == layout::LayoutDirection::RightToLeft ? -1.0f : 1.0f, 1};
  setOffset(_offset + event.delta * direction * _props.wheelStep);
  if (previous != _offset) {
    event.delta -= (_offset - previous) / (_props.wheelStep * direction);
    event.handled = true;
    if (math::almostEqual(event.delta, {}))
      event.stopPropagation();
  }
}

VirtualGrid::VirtualGrid(std::shared_ptr<const CollectionSource> source,
                         ItemFactory factory, VirtualGridProps props,
                         layout::BoxProps box)
    : KeyedChildren{std::move(source), std::move(factory), box}, _props{props} {
  _props.validate();
  rebuildIndex();
  setClip(true);
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
}

void VirtualGrid::applyPatch(const VirtualGridPatch &p) {
  const VirtualGridProps d;
  setProps({p.columns.appliedTo(_props.columns, d.columns),
            p.cellExtent.appliedTo(_props.cellExtent, d.cellExtent),
            p.gap.appliedTo(_props.gap, d.gap),
            p.overscan.appliedTo(_props.overscan, d.overscan),
            p.wheelStep.appliedTo(_props.wheelStep, d.wheelStep)});
}

void VirtualGrid::setProps(VirtualGridProps props) {
  props.validate();
  if (props == _props)
    return;
  const auto saved = anchor();
  const auto previousProps = std::exchange(_props, props);
  try {
    rebuildIndex();
  } catch (...) {
    _props = previousProps;
    throw;
  }
  restoreAnchor(saved);
  invalidateLayout();
}

void VirtualGrid::setOffset(math::Vec2f value) {
  if (!math::isFinite(value))
    throw std::invalid_argument("Virtual grid offset must be finite");
  _offset = value;
  clampOffset();
  invalidateLayout();
}

void VirtualGrid::scrollToCell(std::size_t row, std::size_t column) {
  if (row >= rows() || column >= _props.columns ||
      row * _props.columns + column >= _keys.size())
    throw std::out_of_range("Virtual grid cell outside collection");
  setOffset({layout::detail::checked(column * strideX()),
             layout::detail::checked(row * strideY())});
}

void VirtualGrid::applyChanges(const CollectionChangeSet &batch) {
  validateChanges(batch);
  applyCollectionChanges();
  finishChanges(batch);
}

void VirtualGrid::applyCollectionChanges() {
  checkStructuralMutation();
  auto keys = detail::collectionKeys(*_source);
  const auto saved = anchor();
  auto previousKeys = std::move(_keys);
  auto previousAvailable = std::move(_availableKeys);
  try {
    replaceKeys(std::move(keys));
    rebuildIndex();
  } catch (...) {
    _keys = std::move(previousKeys);
    _availableKeys = std::move(previousAvailable);
    throw;
  }
  restoreAnchor(saved);
  _sourceRevision = _source->revision();
  invalidateLayout();
}

} // namespace playground::ui
