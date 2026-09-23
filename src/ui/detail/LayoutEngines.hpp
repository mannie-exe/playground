#pragma once

#include <ui/containers/Flow.hpp>
#include <ui/containers/Grid.hpp>
#include <ui/containers/Stack.hpp>

namespace playground::ui {

namespace container_detail {

struct StackPlanItem {
  std::size_t index;
  math::Insets margin;
  layout::CrossAlignment alignment;
  layout::MeasureResult measured;
  float mainPosition{};
  float crossPosition{};
};

struct StackPlan {
  std::vector<StackPlanItem> items;
  math::Size2 size;
  std::optional<float> firstBaseline;
  std::optional<float> lastBaseline;
};

inline float baseline(const StackPlanItem &item, bool last) {
  return (last ? item.measured.lastBaseline : item.measured.firstBaseline)
      .value_or(item.measured.size.height);
}

template <typename PlacementAt>
StackPlan stackPlan(Node &parent, std::span<const std::size_t> indices,
                    PlacementAt placementAt, MeasureContext &context,
                    const layout::SizeConstraints &offered, layout::Axis axis,
                    const layout::StackProps &props, bool arranging = false) {
  StackPlan plan;
  std::vector<layout::FlexItem> flex;
  float margins{};
  const auto mainOffered = main(offered, axis);
  const auto crossOffered = cross(offered, axis);

  for (const auto index : indices) {
    auto &child = *parent.children()[index];
    if (child.visibility() == Visibility::Collapsed)
      continue;
    const auto &placement = placementAt(index);
    const auto alignment =
        placement.crossAlignmentOverride.value_or(props.childrenAlignment);
    const float mainMargin = marginMain(placement.margin, axis);
    const float crossMargin = marginCross(placement.margin, axis);
    auto initialMain = layout::AxisConstraints::unbounded();
    const auto kind = rule(child, axis).kind();
    if (kind == layout::SizeKind::Percent || kind == layout::SizeKind::Fill)
      initialMain = {0, mainOffered.deflated(mainMargin).maximum};
    auto measured = child.measure(
        context,
        constraints(initialMain,
                    {0, crossOffered.deflated(crossMargin).maximum}, axis));
    plan.items.push_back({index, placement.margin, alignment, measured});
    flex.push_back({main(measured.size, axis), limits(child, axis),
                    placement.grow, placement.shrink,
                    kind == layout::SizeKind::Fixed});
    margins = sum(margins, mainMargin);
  }

  double natural =
      margins +
      (flex.empty() ? 0.0 : static_cast<double>(props.gap) * (flex.size() - 1));
  for (const auto &item : flex)
    natural += item.limits.clamp(item.basis);
  const float available =
      mainOffered.maximum.value_or(layout::detail::checked(natural));
  auto allocation = layout::allocateStack(
      flex, std::max(0.0f, available - margins), props.gap);
  float mainExtent = margins;
  if (flex.size() > 1)
    mainExtent =
        sum(mainExtent, layout::detail::checked(static_cast<double>(props.gap) *
                                                (flex.size() - 1)));
  float crossExtent{};
  float firstAscent{}, firstDescent{}, lastAscent{}, lastDescent{};
  bool firstGroup{}, lastGroup{};

  for (std::size_t i = 0; i < plan.items.size(); ++i) {
    auto &item = plan.items[i];
    auto &child = *parent.children()[item.index];
    auto offeredCross = layout::AxisConstraints{
        0, crossOffered.deflated(marginCross(item.margin, axis)).maximum};
    const auto crossAxis = axis == layout::Axis::Horizontal
                               ? layout::Axis::Vertical
                               : layout::Axis::Horizontal;
    if (arranging && item.alignment == layout::CrossAlignment::Stretch &&
        offeredCross.maximum &&
        rule(child, crossAxis).kind() != layout::SizeKind::Fixed)
      offeredCross = layout::AxisConstraints::tight(
          limits(child, crossAxis).clamp(*offeredCross.maximum));
    item.measured = child.measure(
        context,
        constraints(layout::AxisConstraints::tight(allocation.sizes[i]),
                    offeredCross, axis));
    mainExtent = sum(mainExtent, main(item.measured.size, axis));
    crossExtent = std::max(crossExtent, sum(cross(item.measured.size, axis),
                                            marginCross(item.margin, axis)));
    if (axis == layout::Axis::Horizontal &&
        (item.alignment == layout::CrossAlignment::FirstBaseline ||
         item.alignment == layout::CrossAlignment::LastBaseline)) {
      const bool last = item.alignment == layout::CrossAlignment::LastBaseline;
      const float value = baseline(item, last);
      (last ? lastGroup : firstGroup) = true;
      auto &ascent = last ? lastAscent : firstAscent;
      auto &descent = last ? lastDescent : firstDescent;
      ascent = std::max(ascent, sum(item.margin.top, value));
      descent = std::max(
          descent, sum(item.margin.bottom, item.measured.size.height - value));
    }
  }
  crossExtent = std::max({crossExtent, sum(firstAscent, firstDescent),
                          sum(lastAscent, lastDescent)});
  plan.size = size(mainExtent, crossExtent, axis);
  const float crossAvailable =
      arranging ? crossOffered.maximum.value_or(crossExtent) : crossExtent;
  const auto distribution = layout::distributionOffsets(
      props.distribution, available - mainExtent, plan.items.size(), props.gap);
  float cursor = distribution.leading;
  for (auto &item : plan.items) {
    const bool horizontal = axis == layout::Axis::Horizontal;
    const bool rtl = context.direction == layout::LayoutDirection::RightToLeft;
    const float leading = horizontal
                              ? (rtl ? item.margin.right : item.margin.left)
                              : item.margin.top;
    const float trailing = horizontal
                               ? (rtl ? item.margin.left : item.margin.right)
                               : item.margin.bottom;
    const float extent = main(item.measured.size, axis);
    const float logical = sum(cursor, leading);
    item.mainPosition =
        horizontal && rtl ? available - logical - extent : logical;
    cursor = sum(sum(logical, extent), sum(trailing, distribution.between));

    const bool baselineAligned =
        horizontal &&
        (item.alignment == layout::CrossAlignment::FirstBaseline ||
         item.alignment == layout::CrossAlignment::LastBaseline);
    if (baselineAligned) {
      const bool last = item.alignment == layout::CrossAlignment::LastBaseline;
      item.crossPosition =
          (last ? lastAscent : firstAscent) - baseline(item, last);
    } else {
      layout::Align align = layout::Align::Start;
      if (item.alignment == layout::CrossAlignment::Center)
        align = layout::Align::Center;
      if (item.alignment == layout::CrossAlignment::End)
        align = layout::Align::End;
      const float low = horizontal ? item.margin.top : item.margin.left;
      const float high = horizontal ? item.margin.bottom : item.margin.right;
      item.crossPosition =
          low + layout::alignmentOffset(
                    align, std::max(0.0f, crossAvailable - low - high),
                    cross(item.measured.size, axis), !horizontal && rtl);
    }
    if (horizontal && item.measured.firstBaseline)
      plan.firstBaseline = plan.firstBaseline.value_or(
          item.crossPosition + *item.measured.firstBaseline);
    if (horizontal && item.measured.lastBaseline)
      plan.lastBaseline = item.crossPosition + *item.measured.lastBaseline;
  }
  if (firstGroup)
    plan.firstBaseline = firstAscent;
  if (lastGroup)
    plan.lastBaseline = lastAscent;
  return plan;
}

inline std::vector<std::size_t> childIndices(const Node &node) {
  std::vector<std::size_t> result(node.children().size());
  std::iota(result.begin(), result.end(), std::size_t{});
  return result;
}

inline void arrangeStackPlan(Node &parent, ArrangeContext &context,
                             const StackPlan &plan, math::Rect bounds,
                             layout::Axis axis) {
  for (const auto &item : plan.items) {
    const auto offset = size(item.mainPosition, item.crossPosition, axis);
    parent.children()[item.index]->arrange(
        context,
        {{bounds.position.x + offset.width, bounds.position.y + offset.height},
         item.measured.size});
  }
}

} // namespace container_detail

namespace container_detail {
template <typename PlacementAt> class GridEngine {
  Node &_owner;
  const layout::GridProps &_props;
  PlacementAt _placementAt;

  auto children() const { return _owner.children(); }
  const layout::GridPlacement &placementInParent(std::size_t i) const {
    return _placementAt(i);
  }
  struct Cell {
    std::size_t child, row, column, rows, columns;
  };
  struct Plan {
    std::vector<Cell> cells;
    std::vector<float> widths, heights;
    math::Size2 size;
  };

  static bool overlaps(const Cell &a, const Cell &b) {
    return a.row < b.row + b.rows && b.row < a.row + a.rows &&
           a.column < b.column + b.columns && b.column < a.column + a.columns;
  }
  static float total(std::span<const float> values, float gap) {
    double result =
        values.empty() ? 0 : static_cast<double>(gap) * (values.size() - 1);
    for (float value : values)
      result += value;
    return layout::detail::checked(result);
  }
  static float start(std::span<const float> values, std::size_t index,
                     float gap) {
    double result = static_cast<double>(gap) * index;
    for (std::size_t i = 0; i < index; ++i)
      result += values[i];
    return layout::detail::checked(result);
  }
  static void contribution(std::vector<float> &sizes,
                           const std::vector<layout::TrackSize> &tracks,
                           std::size_t begin, std::size_t count, float required,
                           float gap) {
    double remaining =
        required - total(std::span{sizes}.subspan(begin, count), gap);
    for (std::size_t pass = 0; pass < count && remaining > 0; ++pass) {
      std::vector<std::size_t> eligible;
      for (std::size_t i = begin; i < begin + count; ++i)
        if (tracks[i].kind() != layout::TrackKind::Fixed &&
            (!tracks[i].limits().maximum ||
             sizes[i] < *tracks[i].limits().maximum))
          eligible.push_back(i);
      if (eligible.empty())
        break;
      const double amount = remaining / eligible.size();
      double applied{};
      for (auto i : eligible) {
        const float next = tracks[i].limits().clamp(
            layout::detail::checked(sizes[i] + amount));
        applied += next - sizes[i];
        sizes[i] = next;
      }
      if (applied <= 0)
        break;
      remaining -= applied;
    }
  }
  static void distribute(std::vector<float> &sizes,
                         const std::vector<layout::TrackSize> &tracks,
                         std::optional<float> available, float gap) {
    if (!available || *available <= total(sizes, gap))
      return;
    std::vector<layout::FlexItem> items;
    for (std::size_t i = 0; i < sizes.size(); ++i)
      items.push_back({sizes[i], tracks[i].limits(),
                       tracks[i].kind() == layout::TrackKind::Fraction
                           ? tracks[i].value()
                           : 0,
                       0, tracks[i].kind() == layout::TrackKind::Fixed});
    sizes = layout::allocateStack(items, *available, gap).sizes;
  }
  Plan makePlan(MeasureContext &context,
                const layout::SizeConstraints &offered) {
    _props.validate(!children().empty());
    Plan plan;
    auto columns = _props.columns, rows = _props.rows;
    auto free = [&](const Cell &cell) {
      return std::none_of(
          plan.cells.begin(), plan.cells.end(),
          [&](const auto &used) { return overlaps(cell, used); });
    };
    auto reserve = [&](Cell cell, bool automatic) {
      if ((!_props.allowOverlap || automatic) && !free(cell))
        throw std::invalid_argument("Grid cells overlap");
      if (cell.row > std::numeric_limits<std::size_t>::max() - cell.rows ||
          cell.column > std::numeric_limits<std::size_t>::max() - cell.columns)
        throw std::length_error("Grid coordinate overflow");
      rows.resize(std::max(rows.size(), cell.row + cell.rows),
                  _props.implicitTrack);
      columns.resize(std::max(columns.size(), cell.column + cell.columns),
                     _props.implicitTrack);
      plan.cells.push_back(cell);
    };
    for (std::size_t i = 0; i < children().size(); ++i) {
      if (children()[i]->visibility() == Visibility::Collapsed)
        continue;
      const auto &p = placementInParent(i);
      if (p.row && p.column)
        reserve({i, *p.row, *p.column, p.rowSpan, p.columnSpan}, false);
    }
    std::size_t cursorRow{}, cursorColumn{};
    for (std::size_t i = 0; i < children().size(); ++i) {
      if (children()[i]->visibility() == Visibility::Collapsed)
        continue;
      const auto &p = placementInParent(i);
      if (p.row && p.column)
        continue;
      Cell cell{i, p.row.value_or(cursorRow), p.column.value_or(cursorColumn),
                p.rowSpan, p.columnSpan};
      if (p.row && !p.column)
        cell.column = 0;
      if (p.column && !p.row)
        cell.row = 0;
      if (!p.row && !p.column) {
        if (_props.autoPlacementAxis == layout::Axis::Horizontal)
          columns.resize(std::max(columns.size(), cell.columns),
                         _props.implicitTrack);
        else
          rows.resize(std::max(rows.size(), cell.rows), _props.implicitTrack);
      }
      for (;;) {
        if (!p.row && !p.column) {
          if (_props.autoPlacementAxis == layout::Axis::Horizontal &&
              cell.column + cell.columns > columns.size()) {
            cell.column = 0;
            ++cell.row;
            if (cell.columns > columns.size())
              columns.resize(cell.columns, _props.implicitTrack);
          } else if (_props.autoPlacementAxis == layout::Axis::Vertical &&
                     cell.row + cell.rows > rows.size()) {
            cell.row = 0;
            ++cell.column;
            if (cell.rows > rows.size())
              rows.resize(cell.rows, _props.implicitTrack);
          }
        }
        if (free(cell))
          break;
        if (p.row || (!p.column &&
                      _props.autoPlacementAxis == layout::Axis::Horizontal)) {
          if (cell.column ==
              std::numeric_limits<std::size_t>::max() - cell.columns)
            throw std::length_error("Grid index overflow");
          ++cell.column;
        } else {
          if (cell.row == std::numeric_limits<std::size_t>::max() - cell.rows)
            throw std::length_error("Grid index overflow");
          ++cell.row;
        }
      }
      reserve(cell, true);
      cursorRow = cell.row;
      cursorColumn = cell.column;
      if (_props.autoPlacementAxis == layout::Axis::Horizontal)
        cursorColumn += cell.columns;
      else
        cursorRow += cell.rows;
    }
    std::sort(plan.cells.begin(), plan.cells.end(),
              [](const Cell &a, const Cell &b) { return a.child < b.child; });
    for (const auto &track : columns)
      plan.widths.push_back(track.limits().minimum);
    for (const auto &track : rows)
      plan.heights.push_back(track.limits().minimum);
    std::vector<std::size_t> order(plan.cells.size());
    std::iota(order.begin(), order.end(), std::size_t{});
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
      return plan.cells[a].columns < plan.cells[b].columns;
    });
    for (auto index : order) {
      const auto &cell = plan.cells[index];
      const auto &p = placementInParent(cell.child);
      auto &child = *children()[cell.child];
      layout::SizeConstraints constraints;
      if (child.boxProps().width.kind() == layout::SizeKind::Percent)
        constraints.width.maximum = offered.width.maximum;
      const auto measured = child.measure(context, constraints);
      contribution(
          plan.widths, columns, cell.column, cell.columns,
          container_detail::sum(measured.size.width, p.margin.horizontal()),
          _props.gap.horizontal);
    }
    distribute(plan.widths, columns, offered.width.maximum,
               _props.gap.horizontal);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
      return plan.cells[a].rows < plan.cells[b].rows;
    });
    for (auto index : order) {
      const auto &cell = plan.cells[index];
      const auto &p = placementInParent(cell.child);
      auto &child = *children()[cell.child];
      const float spanWidth =
          total(std::span{plan.widths}.subspan(cell.column, cell.columns),
                _props.gap.horizontal);
      auto width = layout::AxisConstraints{
          0, std::max(0.0f, spanWidth - p.margin.horizontal())};
      const auto align = container_detail::eligibleAlignment(
          child, p.alignmentOverride.value_or(_props.childrenAlignment));
      if (align.horizontal == layout::Align::Stretch)
        width = layout::AxisConstraints::tight(
            container_detail::limits(child, layout::Axis::Horizontal)
                .clamp(*width.maximum));
      const auto measured = child.measure(context, {width, {}});
      contribution(
          plan.heights, rows, cell.row, cell.rows,
          container_detail::sum(measured.size.height, p.margin.vertical()),
          _props.gap.vertical);
    }
    distribute(plan.heights, rows, offered.height.maximum, _props.gap.vertical);
    plan.size = {total(plan.widths, _props.gap.horizontal),
                 total(plan.heights, _props.gap.vertical)};
    return plan;
  }

public:
  GridEngine(Node &owner, const layout::GridProps &props, PlacementAt placement)
      : _owner{owner}, _props{props}, _placementAt{std::move(placement)} {}
  layout::MeasureResult measure(MeasureContext &context,
                                const layout::SizeConstraints &offered) {
    return {makePlan(context, offered).size};
  }
  void arrange(ArrangeContext &context, math::Rect bounds) {
    const auto plan =
        makePlan(context, layout::SizeConstraints::tight(bounds.size));
    for (const auto &cell : plan.cells) {
      const float width =
          total(std::span{plan.widths}.subspan(cell.column, cell.columns),
                _props.gap.horizontal);
      const float height =
          total(std::span{plan.heights}.subspan(cell.row, cell.rows),
                _props.gap.vertical);
      float x = start(plan.widths, cell.column, _props.gap.horizontal);
      if (context.direction == layout::LayoutDirection::RightToLeft)
        x = bounds.w() - x - width;
      const auto &p = placementInParent(cell.child);
      container_detail::placeAligned(
          *children()[cell.child], context,
          math::rect(bounds.x() + x,
                     bounds.y() +
                         start(plan.heights, cell.row, _props.gap.vertical),
                     width, height),
          p.margin, p.alignmentOverride.value_or(_props.childrenAlignment));
    }
  }
};
} // namespace container_detail

namespace container_detail {
template <typename PlacementAt> class FlowEngine {
  Node &_owner;
  const layout::FlowProps &_props;
  PlacementAt _placementAt;
  auto children() const { return _owner.children(); }
  const layout::StackPlacement &placementInParent(std::size_t i) const {
    return _placementAt(i);
  }

  struct Plan {
    std::vector<container_detail::StackPlan> lines;
    math::Size2 size;
  };

  Plan makePlan(MeasureContext &context,
                const layout::SizeConstraints &offered) {
    using namespace container_detail;
    std::vector<std::size_t> indices;
    std::vector<float> bases;
    const auto mainOffered = main(offered, _props.mainAxis);
    const auto crossOffered = cross(offered, _props.mainAxis);
    for (std::size_t i = 0; i < children().size(); ++i) {
      if (children()[i]->visibility() == Visibility::Collapsed)
        continue;
      const auto &placement = placementInParent(i);
      auto mainConstraint = layout::AxisConstraints{};
      const auto kind = rule(*children()[i], _props.mainAxis).kind();
      if (kind == layout::SizeKind::Percent || kind == layout::SizeKind::Fill)
        mainConstraint.maximum =
            mainOffered.deflated(marginMain(placement.margin, _props.mainAxis))
                .maximum;
      const auto measured = children()[i]->measure(
          context, constraints(mainConstraint,
                               {0, crossOffered
                                       .deflated(marginCross(placement.margin,
                                                             _props.mainAxis))
                                       .maximum},
                               _props.mainAxis));
      indices.push_back(i);
      bases.push_back(sum(main(measured.size, _props.mainAxis),
                          marginMain(placement.margin, _props.mainAxis)));
    }
    const auto breaks =
        layout::breakFlowLines(bases, mainOffered.maximum, _props.itemGap);
    Plan result;
    float mainExtent{}, crossExtent{};
    const layout::StackProps stackProps{_props.itemGap, _props.distribution,
                                        _props.childrenAlignment};
    for (const auto &line : breaks) {
      auto plan = stackPlan(
          _owner, std::span{indices}.subspan(line.begin, line.end - line.begin),
          [&](auto index) -> const auto & { return placementInParent(index); },
          context,
          constraints({0, mainOffered.maximum}, {0, crossOffered.maximum},
                      _props.mainAxis),
          _props.mainAxis, stackProps);
      mainExtent = std::max(mainExtent, main(plan.size, _props.mainAxis));
      if (!result.lines.empty())
        crossExtent = sum(crossExtent, _props.lineGap);
      crossExtent = sum(crossExtent, cross(plan.size, _props.mainAxis));
      result.lines.push_back(std::move(plan));
    }
    result.size = size(mainExtent, crossExtent, _props.mainAxis);
    return result;
  }

public:
  FlowEngine(Node &owner, const layout::FlowProps &props, PlacementAt placement)
      : _owner{owner}, _props{props}, _placementAt{std::move(placement)} {}
  layout::MeasureResult measure(MeasureContext &context,
                                const layout::SizeConstraints &offered) {
    return {makePlan(context, offered).size};
  }
  void arrange(ArrangeContext &context, math::Rect bounds) {
    using namespace container_detail;
    auto plan = makePlan(context, layout::SizeConstraints::tight(bounds.size));
    float cursor{};
    for (const auto &line : plan.lines) {
      std::vector<std::size_t> indices;
      for (const auto &item : line.items)
        indices.push_back(item.index);
      const auto lineSize =
          size(main(bounds.size, _props.mainAxis),
               cross(line.size, _props.mainAxis), _props.mainAxis);
      auto arranged = stackPlan(
          _owner, indices,
          [&](auto index) -> const auto & { return placementInParent(index); },
          context, layout::SizeConstraints::tight(lineSize), _props.mainAxis,
          {_props.itemGap, _props.distribution, _props.childrenAlignment},
          true);
      const float crossPosition =
          _props.mainAxis == layout::Axis::Vertical &&
                  context.direction == layout::LayoutDirection::RightToLeft
              ? bounds.size.width - cursor - lineSize.width
              : cursor;
      const auto offset = size(0, crossPosition, _props.mainAxis);
      arrangeStackPlan(_owner, context, arranged,
                       {{bounds.position.x + offset.width,
                         bounds.position.y + offset.height},
                        lineSize},
                       _props.mainAxis);
      cursor =
          sum(cursor, sum(cross(line.size, _props.mainAxis), _props.lineGap));
    }
  }
};
} // namespace container_detail

} // namespace playground::ui
