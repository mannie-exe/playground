#pragma once

#include <numeric>

#include <ui/containers/Container.hpp>

namespace playground::ui {

struct GridPatch {
  Patch<std::vector<layout::TrackSize>> columns;
  Patch<std::vector<layout::TrackSize>> rows;
  Patch<math::Gap2> gap;
  Patch<layout::Axis> autoPlacementAxis;
  Patch<layout::Alignment> childrenAlignment;
  Patch<bool> allowOverlap;
  Patch<layout::TrackSize> implicitTrack;
};

struct GridPlacementPatch {
  Patch<std::optional<std::size_t>> row;
  Patch<std::optional<std::size_t>> column;
  Patch<std::size_t> rowSpan;
  Patch<std::size_t> columnSpan;
  Patch<math::Insets> margin;
  Patch<std::optional<layout::Alignment>> alignmentOverride;
};

class Grid : public PlacementContainer<layout::GridPlacement> {
  layout::GridProps _props;

protected:
  void validatePlacementAt(const layout::GridPlacement &p,
                           std::optional<std::size_t> replacing) const override;
  layout::MeasureResult
  measureContent(MeasureContext &context,
                 const layout::SizeConstraints &offered) override;
  void arrangeChildren(ArrangeContext &context, math::Rect bounds) override;

public:
  explicit Grid(layout::GridProps props = {}, layout::BoxProps box = {})
      : PlacementContainer{box}, _props{std::move(props)} {
    _props.validate(false);
  }
  const layout::GridProps &props() const noexcept { return _props; }
  void setProps(layout::GridProps value);
  void applyPatch(const GridPatch &p);
  void applyPlacementPatch(NodeId child, const GridPlacementPatch &patch) {
    applyPlacementPatch(indexOf(child), patch);
  }
  void applyPlacementPatch(std::size_t index, const GridPlacementPatch &p);
  void setTracks(std::vector<layout::TrackSize> columns,
                 std::vector<layout::TrackSize> rows);
  void setGap(math::Gap2 gap);
};

} // namespace playground::ui
