#include "../detail/LayoutEngines.hpp"
#include <ui/containers/Grid.hpp>

namespace playground::ui {

void Grid::validatePlacementAt(const layout::GridPlacement &p,
                               std::optional<std::size_t> replacing) const {
  p.validate();
  if (_props.allowOverlap || !p.row || !p.column)
    return;
  for (std::size_t i = 0; i < children().size(); ++i) {
    if (replacing == i)
      continue;
    const auto &v = placementInParent(i);
    if (v.row && v.column && *p.row < *v.row + v.rowSpan &&
        *v.row < *p.row + p.rowSpan && *p.column < *v.column + v.columnSpan &&
        *v.column < *p.column + p.columnSpan)
      throw std::invalid_argument("Explicit grid placements overlap");
  }
}

void Grid::setGridProps(layout::GridProps value) {
  value.validate(!children().empty());
  if (value == _props)
    return;
  if (!value.allowOverlap) {
    for (std::size_t i = 0; i < children().size(); ++i) {
      const auto &a = placementInParent(i);
      if (!a.row || !a.column)
        continue;
      for (std::size_t j = 0; j < i; ++j) {
        const auto &b = placementInParent(j);
        if (b.row && b.column && *a.row < *b.row + b.rowSpan &&
            *b.row < *a.row + a.rowSpan &&
            *a.column < *b.column + b.columnSpan &&
            *b.column < *a.column + a.columnSpan)
          throw std::invalid_argument(
              "Existing explicit grid placements overlap");
      }
    }
  }
  _props = std::move(value);
  invalidateLayout();
}

void Grid::applyGridPatch(const GridPatch &p) {
  const layout::GridProps d;
  setGridProps(
      {p.columns.appliedTo(_props.columns, d.columns),
       p.rows.appliedTo(_props.rows, d.rows),
       p.gap.appliedTo(_props.gap, d.gap),
       p.autoPlacementAxis.appliedTo(_props.autoPlacementAxis,
                                     d.autoPlacementAxis),
       p.childrenAlignment.appliedTo(_props.childrenAlignment,
                                     d.childrenAlignment),
       p.allowOverlap.appliedTo(_props.allowOverlap, d.allowOverlap),
       p.implicitTrack.appliedTo(_props.implicitTrack, d.implicitTrack)});
}

void Grid::applyPlacementPatch(std::size_t index, const GridPlacementPatch &p) {
  const auto &v = placementInParent(index);
  const layout::GridPlacement d;
  setPlacementInParent(index,
                       {p.row.appliedTo(v.row, d.row),
                        p.column.appliedTo(v.column, d.column),
                        p.rowSpan.appliedTo(v.rowSpan, d.rowSpan),
                        p.columnSpan.appliedTo(v.columnSpan, d.columnSpan),
                        p.margin.appliedTo(v.margin, d.margin),
                        p.alignmentOverride.appliedTo(v.alignmentOverride,
                                                      d.alignmentOverride)});
}

void Grid::setTracks(std::vector<layout::TrackSize> columns,
                     std::vector<layout::TrackSize> rows) {
  auto p = _props;
  p.columns = std::move(columns);
  p.rows = std::move(rows);
  setGridProps(std::move(p));
}

void Grid::setGap(math::Gap2 gap) {
  auto p = _props;
  p.gap = gap;
  setGridProps(std::move(p));
}

layout::MeasureResult
Grid::measureContent(MeasureContext &context,
                     const layout::SizeConstraints &offered) {
  auto engine = container_detail::GridEngine{
      *this, _props, [this](std::size_t i) -> const layout::GridPlacement & {
        return placementInParent(i);
      }};
  return engine.measure(context, offered);
}

void Grid::arrangeChildren(ArrangeContext &context, math::Rect bounds) {
  auto engine = container_detail::GridEngine{
      *this, _props, [this](std::size_t i) -> const layout::GridPlacement & {
        return placementInParent(i);
      }};
  engine.arrange(context, bounds);
}

} // namespace playground::ui
