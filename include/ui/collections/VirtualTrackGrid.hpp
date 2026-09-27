#pragma once

#include <numeric>

#include <ui/collections/Collection.hpp>
#include <ui/collections/ExtentIndex.hpp>
#include <ui/containers/Boundaries.hpp>

namespace playground::ui {

struct TrackExtent {
  float value{32};
  bool estimated{};
  layout::AxisConstraints limits;
  bool operator==(const TrackExtent &) const = default;

  void validate() const {
    limits.validate();
    if (!std::isfinite(value) || value <= 0)
      throw std::invalid_argument("Track extent must be finite and positive");
  }
};

struct GridItemPlacement {
  std::size_t row{}, column{}, rowSpan{1}, columnSpan{1};
};

class GridSource : public CollectionSource {
public:
  virtual GridItemPlacement placementAt(std::size_t index) const = 0;
};

struct FrozenTracks {
  std::size_t rowsStart{}, rowsEnd{}, columnsStart{}, columnsEnd{};
  bool operator==(const FrozenTracks &) const = default;
};

struct VirtualTrackGridProps {
  std::vector<TrackExtent> columns{{80}};
  std::vector<TrackExtent> rows{{32}};
  math::Gap2 gap;
  FrozenTracks frozen;
  float overscan{64};
  float wheelStep{32};
  bool operator==(const VirtualTrackGridProps &) const = default;
};

struct VirtualTrackGridPatch {
  Patch<std::vector<TrackExtent>> columns, rows;
  Patch<math::Gap2> gap;
  Patch<FrozenTracks> frozen;
  Patch<float> overscan, wheelStep;
};

// Explicit-track counterpart to the uniform VirtualGrid. Each live item has a
// Clip wrapper owned once; pane clipping never overwrites the item's own props.
class VirtualTrackGrid : public detail::KeyedChildren {
  VirtualTrackGridProps _props;
  std::shared_ptr<const GridSource> _gridSource;

  detail::ExtentIndex _columns, _rows;
  std::vector<GridItemPlacement> _placements;
  std::unordered_map<ItemKey, std::size_t> _indices;
  std::vector<std::size_t> _rowOrder;
  std::vector<std::size_t> _maxEndRow;
  std::vector<std::size_t> _frozenRowItems;
  math::Vec2f _offset;
  math::Size2 _viewport;
  std::vector<ItemKey> _visible;
  bool _refinementPending{};
  layout::LayoutDirection _direction{};

  static ItemFactory wrapped(ItemFactory factory);

  static void validate(const VirtualTrackGridProps &p);

  void rebuild(bool resetExtents);

  struct AxisCell {
    float position, extent, clipStart, clipEnd;
  };

  AxisCell axis(const detail::ExtentIndex &index, std::size_t start,
                std::size_t span, std::size_t leading, std::size_t trailing,
                float gap, float viewport, float offset) const;

  std::pair<math::Rect, math::Rect> rectangles(std::size_t i) const;

  void clampOffset();

protected:
  void prepareChildren(MeasureContext &context,
                       const layout::SizeConstraints &offered) override;

  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void arrangeChildren(ArrangeContext &context, math::Rect content) override;
  void onDefaultEvent(UIEvent &event) override;

public:
  VirtualTrackGrid(std::shared_ptr<const GridSource> source,
                   ItemFactory factory, VirtualTrackGridProps props = {},
                   layout::BoxProps box = {});

  const VirtualTrackGridProps &props() const noexcept { return _props; }

  void setProps(VirtualTrackGridProps props);
  void applyPatch(const VirtualTrackGridPatch &p);

  math::Size2 contentExtent() const {
    return {layout::detail::checked(_columns.total() - _props.gap.horizontal),
            layout::detail::checked(_rows.total() - _props.gap.vertical)};
  }

  math::Vec2f offset() const noexcept { return _offset; }

  std::span<const ItemKey> visibleKeys() const noexcept { return _visible; }

  Node *item(const ItemKey &key) const {
    auto *wrapper = realized(key);
    return wrapper ? wrapper->children().front().get() : nullptr;
  }

  void setOffset(math::Vec2f offset);
  void scrollToCell(std::size_t row, std::size_t column);
  void applyChanges(const CollectionChangeSet &batch);
};

} // namespace playground::ui
