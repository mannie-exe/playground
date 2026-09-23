#include <ui/collections/VirtualTrackGrid.hpp>

namespace playground::ui {

ItemFactory VirtualTrackGrid::wrapped(ItemFactory factory) {
  if (!factory.create)
    throw std::invalid_argument("Grid requires an item factory");
  return {
      [create = factory.create](const ItemKey &key) -> std::unique_ptr<Node> {
        auto child = create(key);
        if (!child)
          throw std::invalid_argument("Grid item factory returned null");
        auto box = std::make_unique<Clip>(std::move(child));
        box->setContentAlignment(layout::Alignment::stretch());
        return box;
      },
      [update = factory.update](Node &node, const ItemKey &key) {
        if (update)
          update(*node.children().front(), key);
      }};
}

void VirtualTrackGrid::validate(const VirtualTrackGridProps &p) {
  if (p.rows.empty() || p.columns.empty())
    throw std::invalid_argument("Virtual track grid needs both axes");
  for (const auto &track : p.rows)
    track.validate();
  for (const auto &track : p.columns)
    track.validate();
  if (p.frozen.rowsStart > p.rows.size() ||
      p.frozen.rowsEnd > p.rows.size() - p.frozen.rowsStart ||
      p.frozen.columnsStart > p.columns.size() ||
      p.frozen.columnsEnd > p.columns.size() - p.frozen.columnsStart)
    throw std::invalid_argument("Frozen tracks exceed grid dimensions");
  layout::detail::nonnegative(p.gap.horizontal, "Invalid column gap");
  layout::detail::nonnegative(p.gap.vertical, "Invalid row gap");
  layout::detail::nonnegative(p.overscan, "Invalid grid overscan");
  layout::detail::nonnegative(p.wheelStep, "Invalid wheel step");
}

void VirtualTrackGrid::rebuild(bool resetExtents) {
  validate(_props);
  std::vector<GridItemPlacement> placements;
  std::unordered_map<ItemKey, std::size_t> indices;
  auto region = [](std::size_t begin, std::size_t span, std::size_t count,
                   std::size_t leading, std::size_t trailing) {
    if (!span || begin >= count || span > count - begin)
      throw std::invalid_argument("Grid span exceeds tracks");
    if ((begin < leading && begin + span > leading) ||
        (begin < count - trailing && begin + span > count - trailing))
      throw std::invalid_argument("Grid span crosses a frozen boundary");
  };
  for (std::size_t i = 0; i < _keys.size(); ++i) {
    auto p = _gridSource->placementAt(i);
    region(p.row, p.rowSpan, _props.rows.size(), _props.frozen.rowsStart,
           _props.frozen.rowsEnd);
    region(p.column, p.columnSpan, _props.columns.size(),
           _props.frozen.columnsStart, _props.frozen.columnsEnd);
    placements.push_back(p);
    indices.emplace(_keys[i], i);
  }
  std::vector<std::size_t> order(_keys.size()), ends, frozen;
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
    return placements[a].row < placements[b].row;
  });
  std::size_t end{};
  for (auto i : order) {
    end = std::max(end, placements[i].row + placements[i].rowSpan);
    ends.push_back(end);
    if (placements[i].row < _props.frozen.rowsStart ||
        placements[i].row >= _props.rows.size() - _props.frozen.rowsEnd)
      frozen.push_back(i);
  }
  detail::ExtentIndex columns = _columns, rows = _rows;
  if (resetExtents) {
    std::vector<double> c, r;
    for (const auto &t : _props.columns)
      c.push_back(std::max(0.01f, t.limits.clamp(t.value)) +
                  _props.gap.horizontal);
    for (const auto &t : _props.rows)
      r.push_back(std::max(0.01f, t.limits.clamp(t.value)) +
                  _props.gap.vertical);
    columns.reset(c);
    rows.reset(r);
  }
  (void)layout::detail::checked(columns.total());
  (void)layout::detail::checked(rows.total());
  _placements = std::move(placements);
  _indices = std::move(indices);
  _rowOrder = std::move(order);
  _maxEndRow = std::move(ends);
  _frozenRowItems = std::move(frozen);
  _columns = std::move(columns);
  _rows = std::move(rows);
}

VirtualTrackGrid::AxisCell
VirtualTrackGrid::axis(const detail::ExtentIndex &index, std::size_t start,
                       std::size_t span, std::size_t leading,
                       std::size_t trailing, float gap, float viewport,
                       float offset) const {
  const double a = index.prefix(leading) - (leading == index.size() ? gap : 0);
  const double b = trailing
                       ? index.total() - index.prefix(index.size() - trailing) -
                             (trailing == index.size() ? gap : 0)
                       : 0;
  if (a + b > viewport)
    throw std::invalid_argument("Frozen tracks do not fit the viewport");
  const auto position = index.prefix(start);
  const auto extent = index.prefix(start + span) - position - gap;
  if (start < leading)
    return {float(position), float(extent), 0, float(a)};
  if (start >= index.size() - trailing)
    return {float(viewport - (index.total() - gap) + position), float(extent),
            float(viewport - b), viewport};
  return {float(position - offset), float(extent), float(a),
          float(viewport - b)};
}

std::pair<math::Rect, math::Rect>
VirtualTrackGrid::rectangles(std::size_t i) const {
  const auto p = _placements[i];
  const auto x = axis(_columns, p.column, p.columnSpan,
                      _props.frozen.columnsStart, _props.frozen.columnsEnd,
                      _props.gap.horizontal, _viewport.width, _offset.x);
  const auto y = axis(_rows, p.row, p.rowSpan, _props.frozen.rowsStart,
                      _props.frozen.rowsEnd, _props.gap.vertical,
                      _viewport.height, _offset.y);
  math::Rect bounds = math::rect(x.position, y.position, x.extent, y.extent);
  math::Rect clip =
      math::rect(x.clipStart, y.clipStart, x.clipEnd - x.clipStart,
                 y.clipEnd - y.clipStart);
  if (_direction == layout::LayoutDirection::RightToLeft) {
    bounds.position.x = _viewport.width - bounds.right();
    clip.position.x = _viewport.width - clip.right();
  }
  return {bounds, clip};
}

void VirtualTrackGrid::clampOffset() {
  const auto extent = contentExtent();
  _offset.x = std::clamp(_offset.x, 0.0f,
                         std::max(0.0f, extent.width - _viewport.width));
  _offset.y = std::clamp(_offset.y, 0.0f,
                         std::max(0.0f, extent.height - _viewport.height));
}

void VirtualTrackGrid::prepareChildren(MeasureContext &context,
                                       const layout::SizeConstraints &offered) {
  if (!offered.width.maximum || !offered.height.maximum)
    throw std::invalid_argument("Virtual track grid needs finite viewport");
  _viewport = {*offered.width.maximum, *offered.height.maximum};
  _direction = context.direction;
  clampOffset();
  // Validate pane extents even when there are no model items to visit.
  (void)axis(_columns, 0, 1, _props.frozen.columnsStart,
             _props.frozen.columnsEnd, _props.gap.horizontal, _viewport.width,
             _offset.x);
  (void)axis(_rows, 0, 1, _props.frozen.rowsStart, _props.frozen.rowsEnd,
             _props.gap.vertical, _viewport.height, _offset.y);
  _visible.clear();
  // Prefix maximum ends retain spans whose origins precede the visible range.
  const auto first = _rows.itemAt(std::max(0.0f, _offset.y - _props.overscan));
  const auto last =
      _rows.itemAt(_offset.y + _viewport.height + _props.overscan);
  const auto begin =
      std::upper_bound(_maxEndRow.begin(), _maxEndRow.end(), first) -
      _maxEndRow.begin();
  auto candidates = _frozenRowItems;
  for (std::size_t j = begin; j < _rowOrder.size(); ++j) {
    auto i = _rowOrder[j];
    if (_placements[i].row > last)
      break;
    candidates.push_back(i);
  }
  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());
  for (auto i : candidates) {
    auto [bounds, clip] = rectangles(i);
    if (math::intersect(bounds,
                        math::outset(clip, math::Insets::all(_props.overscan)))
            .hasArea())
      _visible.push_back(_keys[i]);
  }
  auto wanted = _visible;
  pinActiveItems(wanted);
  reconcile(std::move(wanted));
  const auto anchorRow = _rows.itemAt(_offset.y),
             anchorCol = _columns.itemAt(_offset.x);
  const double beforeY = _rows.prefix(anchorRow),
               beforeX = _columns.prefix(anchorCol);
  bool changed{};
  for (const auto &key : realizedKeys()) {
    const auto i = _indices.at(key);
    const auto p = _placements[i];
    const auto measured = realized(key)->measure(context, {});
    auto grow = [&](detail::ExtentIndex &index,
                    const std::vector<TrackExtent> &tracks, std::size_t start,
                    std::size_t span, float required, float gap) {
      const double existing =
          index.prefix(start + span) - index.prefix(start) - gap;
      if (required <= existing)
        return;
      double remaining = required - existing;
      while (remaining > 0.001) {
        std::size_t flexible{};
        for (auto j = start; j < start + span; ++j)
          flexible +=
              tracks[j].estimated &&
              (!tracks[j].limits.maximum ||
               index.value(j) - gap < *tracks[j].limits.maximum - 0.001);
        if (!flexible)
          break;
        double consumed{};
        for (auto j = start; j < start + span; ++j) {
          if (!tracks[j].estimated ||
              (tracks[j].limits.maximum &&
               index.value(j) - gap >= *tracks[j].limits.maximum - 0.001))
            continue;
          const double old = index.value(j);
          const float next =
              tracks[j].limits.clamp(float(old - gap + remaining / flexible));
          if (next + gap > old + 0.001) {
            index.set(j, next + gap);
            consumed += index.value(j) - old;
            changed = true;
          }
        }
        if (consumed <= 0.001)
          break;
        remaining -= consumed;
      }
      (void)layout::detail::checked(index.total());
    };
    grow(_columns, _props.columns, p.column, p.columnSpan, measured.size.width,
         _props.gap.horizontal);
    // Reflow text against its actual span width before refining row heights.
    const auto width = rectangles(i).first.w();
    const auto reflowed = realized(key)->measure(context, {{0, width}, {}});
    grow(_rows, _props.rows, p.row, p.rowSpan, reflowed.size.height,
         _props.gap.vertical);
  }
  if (changed) {
    _offset.x += float(_columns.prefix(anchorCol) - beforeX);
    _offset.y += float(_rows.prefix(anchorRow) - beforeY);
    _refinementPending = true;
  }
}

layout::MeasureResult
VirtualTrackGrid::measureContent(MeasureContext &,
                                 const layout::SizeConstraints &) {
  // Invalidating during prepareChildren alone can be absorbed by Node's
  // measurement cache. Keep a changed realization range eligible for retry.
  if (std::exchange(_refinementPending, false))
    invalidateLayout();
  return {_viewport};
}

void VirtualTrackGrid::arrangeChildren(ArrangeContext &context,
                                       math::Rect content) {
  _viewport = content.size;
  clampOffset();
  for (const auto &key : realizedKeys()) {
    auto [bounds, region] = rectangles(_indices.at(key));
    auto &wrapper = static_cast<Clip &>(*realized(key));
    auto localClip = math::intersect(bounds, region);
    localClip.position.x -= bounds.x();
    localClip.position.y -= bounds.y();
    wrapper.setClipRect(localClip);
    bounds.position.x += content.x();
    bounds.position.y += content.y();
    wrapper.arrange(context, bounds);
  }
}

void VirtualTrackGrid::onDefaultEvent(UIEvent &event) {
  if (event.type != EventType::Wheel || event.defaultPrevented ||
      !_props.wheelStep)
    return;
  const auto old = _offset;
  const math::Vec2f direction{
      _direction == layout::LayoutDirection::RightToLeft ? -1.0f : 1.0f, 1};
  setOffset(_offset + event.delta * direction * _props.wheelStep);
  if (_offset != old) {
    event.delta -= (_offset - old) / (_props.wheelStep * direction);
    event.handled = true;
    if (math::almostEqual(event.delta, {}))
      event.stopPropagation();
  }
}

VirtualTrackGrid::VirtualTrackGrid(std::shared_ptr<const GridSource> source,
                                   ItemFactory factory,
                                   VirtualTrackGridProps props,
                                   layout::BoxProps box)
    : KeyedChildren{source, wrapped(std::move(factory)), box},
      _props{std::move(props)}, _gridSource{std::move(source)} {
  rebuild(true);
  setClip(true);
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
}

void VirtualTrackGrid::setProps(VirtualTrackGridProps props) {
  validate(props);
  if (props == _props)
    return;
  auto previous = std::exchange(_props, std::move(props));
  try {
    rebuild(true);
  } catch (...) {
    _props = std::move(previous);
    throw;
  }
  clampOffset();
  invalidateLayout();
}

void VirtualTrackGrid::applyPatch(const VirtualTrackGridPatch &p) {
  const VirtualTrackGridProps d;
  setProps({p.columns.appliedTo(_props.columns, d.columns),
            p.rows.appliedTo(_props.rows, d.rows),
            p.gap.appliedTo(_props.gap, d.gap),
            p.frozen.appliedTo(_props.frozen, d.frozen),
            p.overscan.appliedTo(_props.overscan, d.overscan),
            p.wheelStep.appliedTo(_props.wheelStep, d.wheelStep)});
}

void VirtualTrackGrid::setOffset(math::Vec2f offset) {
  if (!math::isFinite(offset))
    throw std::invalid_argument("Invalid grid offset");
  _offset = offset;
  clampOffset();
  invalidateLayout();
}

void VirtualTrackGrid::scrollToCell(std::size_t row, std::size_t column) {
  if (row >= _rows.size() || column >= _columns.size())
    throw std::out_of_range("Grid coordinate");
  setOffset({float(_columns.prefix(column)), float(_rows.prefix(row))});
}

void VirtualTrackGrid::applyChanges(const CollectionChangeSet &batch) {
  checkStructuralMutation();
  validateChanges(batch);
  std::optional<ItemKey> anchor;
  math::Vec2f before;
  for (const auto &key : _visible) {
    if (!_indices.contains(key))
      continue;
    const auto p = _placements[_indices.at(key)];
    if (p.row >= _props.frozen.rowsStart &&
        p.row < _props.rows.size() - _props.frozen.rowsEnd &&
        p.column >= _props.frozen.columnsStart &&
        p.column < _props.columns.size() - _props.frozen.columnsEnd) {
      anchor = key;
      before = {float(_columns.prefix(p.column)), float(_rows.prefix(p.row))};
      break;
    }
  }
  auto keys = detail::collectionKeys(*_source);
  auto previous = _keys;
  replaceKeys(std::move(keys));
  try {
    rebuild(false);
  } catch (...) {
    replaceKeys(std::move(previous));
    throw;
  }
  if (anchor && _indices.contains(*anchor)) {
    const auto p = _placements[_indices.at(*anchor)];
    _offset += math::Vec2f{float(_columns.prefix(p.column)),
                           float(_rows.prefix(p.row))} -
               before;
  }
  clampOffset();
  invalidateLayout();
  finishChanges(batch);
}

} // namespace playground::ui
