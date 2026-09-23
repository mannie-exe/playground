#include <ui/collections/VirtualList.hpp>

namespace playground::ui {

std::optional<VirtualList::Anchor> VirtualList::anchor() const {
  const auto index = _extents.itemAt(_offset);
  if (index >= _keys.size())
    return std::nullopt;
  return Anchor{_keys[index],
                static_cast<float>(_offset - _extents.prefix(index))};
}

void VirtualList::restoreAnchor(const std::optional<Anchor> &value) {
  if (value)
    if (const auto it = _indices.find(value->key); it != _indices.end())
      _offset =
          layout::detail::checked(_extents.prefix(it->second) + value->within);
  clampOffset();
}

void VirtualList::rebuildIndex() {
  std::unordered_map<ItemKey, std::size_t> indices;
  std::vector<double> extents;
  extents.reserve(_keys.size());
  for (std::size_t i = 0; i < _keys.size(); ++i) {
    indices.emplace(_keys[i], i);
    const auto it = _measuredExtents.find(_keys[i]);
    const float value = _props.extentMode == ItemExtentMode::Estimated &&
                                it != _measuredExtents.end()
                            ? it->second
                            : _props.itemExtent;
    extents.push_back(static_cast<double>(value) + _props.gap);
  }
  detail::ExtentIndex index{extents};
  layout::detail::checked(index.total());
  _indices = std::move(indices);
  _extents = std::move(index);
}

std::vector<ItemKey> VirtualList::visibleRange() const {
  std::vector<ItemKey> result;
  if (main(_viewport) <= 0)
    return result;
  const double begin =
      std::max(0.0, static_cast<double>(_offset) - _props.overscan);
  const double end =
      static_cast<double>(_offset) + main(_viewport) + _props.overscan;
  for (auto i = _extents.itemAt(begin);
       i < _keys.size() && _extents.prefix(i) < end; ++i)
    result.push_back(_keys[i]);
  return result;
}

void VirtualList::prepareChildren(MeasureContext &context,
                                  const layout::SizeConstraints &offered) {
  if (!offered.width.maximum || !offered.height.maximum)
    throw std::invalid_argument(
        "VirtualList requires a finite viewport on both axes");
  _viewport = {*offered.width.maximum, *offered.height.maximum};
  const auto savedAnchor = anchor();
  if (_measuredCross != cross(_viewport) ||
      _measuredEnvironment != context.environmentRevision ||
      _measuredDirection != context.direction ||
      _measuredScale != context.pixelScale) {
    _measuredExtents.clear();
    rebuildIndex();
    _measuredCross = cross(_viewport);
    _measuredEnvironment = context.environmentRevision;
    _measuredDirection = context.direction;
    _measuredScale = context.pixelScale;
    restoreAnchor(savedAnchor);
  }
  clampOffset();
  // Bounded convergence: corrections can reveal further items. Any remaining
  // estimate error gets another pass next layout, never an unbounded loop.
  for (int pass = 0; pass < 4; ++pass) {
    _visible = visibleRange();
    auto wanted = _visible;
    pinActiveItems(wanted);
    reconcile(std::move(wanted));
    const auto previousAnchor = anchor();
    bool changed{};
    for (const auto &key : realizedKeys()) {
      const auto index = _indices.at(key);
      const auto mainConstraint =
          _props.extentMode == ItemExtentMode::Fixed
              ? layout::AxisConstraints::tight(_props.itemExtent)
              : layout::AxisConstraints{};
      const auto crossConstraint =
          layout::AxisConstraints::tight(cross(_viewport));
      const layout::SizeConstraints constraints =
          _props.axis == layout::Axis::Vertical
              ? layout::SizeConstraints{crossConstraint, mainConstraint}
              : layout::SizeConstraints{mainConstraint, crossConstraint};
      const auto size = realized(key)->measure(context, constraints).size;
      if (_props.extentMode == ItemExtentMode::Estimated) {
        const float measured = std::max(0.01f, main(size));
        const double stride = static_cast<double>(measured) + _props.gap;
        if (_extents.value(index) != stride) {
          _extents.set(index, stride);
          changed = true;
        }
        _measuredExtents.insert_or_assign(key, measured);
      }
    }
    restoreAnchor(previousAnchor);
    if (!changed || visibleRange() == _visible)
      break;
  }
}

layout::MeasureResult
VirtualList::measureContent(MeasureContext &, const layout::SizeConstraints &) {
  if (visibleRange() != _visible)
    invalidateLayout();
  return {_viewport};
}

void VirtualList::arrangeChildren(ArrangeContext &context, math::Rect content) {
  _viewport = content.size;
  clampOffset();
  for (const auto &key : realizedKeys()) {
    const auto index = _indices.at(key);
    const float start =
        layout::detail::checked(_extents.prefix(index)) - _offset;
    const float length = static_cast<float>(_extents.value(index) - _props.gap);
    const float x = context.direction == layout::LayoutDirection::RightToLeft
                        ? content.right() - start - length
                        : content.x() + start;
    const auto bounds =
        _props.axis == layout::Axis::Vertical
            ? math::rect(content.x(), content.y() + start, content.w(), length)
            : math::rect(x, content.y(), length, content.h());
    realized(key)->arrange(context, bounds);
  }
}

void VirtualList::onDefaultEvent(UIEvent &event) {
  if (event.type != EventType::Wheel || event.phase == EventPhase::Capture ||
      event.defaultPrevented || _props.wheelStep == 0)
    return;
  float &delta =
      _props.axis == layout::Axis::Vertical ? event.delta.y : event.delta.x;
  const float previous = _offset;
  const float direction =
      _props.axis == layout::Axis::Horizontal &&
              _measuredDirection == layout::LayoutDirection::RightToLeft
          ? -1.0f
          : 1.0f;
  setOffset(_offset + delta * _props.wheelStep * direction);
  if (previous != _offset) {
    delta -= (_offset - previous) / (_props.wheelStep * direction);
    event.handled = true;
    if (math::almostEqual(event.delta, {}))
      event.stopPropagation();
  }
}

VirtualList::VirtualList(std::shared_ptr<const CollectionSource> source,
                         ItemFactory factory, VirtualListProps props,
                         layout::BoxProps box)
    : KeyedChildren{std::move(source), std::move(factory), box}, _props{props} {
  _props.validate();
  rebuildIndex();
  setClip(true);
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
}

void VirtualList::applyPatch(const VirtualListPatch &p) {
  const VirtualListProps d;
  setProps({p.axis.appliedTo(_props.axis, d.axis),
            p.extentMode.appliedTo(_props.extentMode, d.extentMode),
            p.itemExtent.appliedTo(_props.itemExtent, d.itemExtent),
            p.gap.appliedTo(_props.gap, d.gap),
            p.overscan.appliedTo(_props.overscan, d.overscan),
            p.wheelStep.appliedTo(_props.wheelStep, d.wheelStep)});
}

void VirtualList::setProps(VirtualListProps props) {
  props.validate();
  if (props == _props)
    return;
  const auto saved = anchor();
  const auto previousProps = std::exchange(_props, props);
  auto previousMeasured = std::move(_measuredExtents);
  _measuredExtents.clear();
  try {
    rebuildIndex();
  } catch (...) {
    _props = previousProps;
    _measuredExtents = std::move(previousMeasured);
    throw;
  }
  restoreAnchor(saved);
  invalidateLayout();
}

void VirtualList::setOffset(float value) {
  if (!std::isfinite(value))
    throw std::invalid_argument("Virtual list offset must be finite");
  _offset = value;
  clampOffset();
  invalidateLayout();
}

void VirtualList::scrollToKey(const ItemKey &key) {
  const auto it = _indices.find(key);
  if (it == _indices.end())
    throw std::out_of_range("Unknown collection key");
  setOffset(layout::detail::checked(_extents.prefix(it->second)));
}

void VirtualList::refreshItem(const ItemKey &key) {
  const auto it = _indices.find(key);
  if (it == _indices.end())
    throw std::out_of_range("Unknown collection key");
  const auto saved = anchor();
  _measuredExtents.erase(key);
  _extents.set(it->second, static_cast<double>(_props.itemExtent) + _props.gap);
  restoreAnchor(saved);
  KeyedChildren::refreshItem(key);
}

void VirtualList::applyChanges(const CollectionChangeSet &batch) {
  validateChanges(batch);
  applyCollectionChanges();
  finishChanges(batch);
}

void VirtualList::applyCollectionChanges() {
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
  std::erase_if(_measuredExtents, [&](const auto &entry) {
    return !_availableKeys.contains(entry.first);
  });
  restoreAnchor(saved);
  _sourceRevision = _source->revision();
  invalidateLayout();
}

} // namespace playground::ui
