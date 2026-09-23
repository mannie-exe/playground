#pragma once

#include <ui/collections/Collection.hpp>

namespace playground::ui {

struct VirtualGridProps {
  std::size_t columns{1};
  math::Size2 cellExtent{80, 32};
  math::Gap2 gap;
  float overscan{64};
  float wheelStep{32};
  bool operator==(const VirtualGridProps &) const = default;
  void validate() const {
    if (!columns || !math::isFinite(cellExtent) || !math::hasArea(cellExtent))
      throw std::invalid_argument("Virtual grid needs positive columns and "
                                  "finite positive cell extents");
    layout::detail::nonnegative(gap.horizontal, "Grid gap must be nonnegative");
    layout::detail::nonnegative(gap.vertical, "Grid gap must be nonnegative");
    layout::detail::nonnegative(overscan, "Grid overscan must be nonnegative");
    layout::detail::nonnegative(wheelStep,
                                "Grid wheel step must be nonnegative");
  }
};

struct VirtualGridPatch {
  Patch<std::size_t> columns;
  Patch<math::Size2> cellExtent;
  Patch<math::Gap2> gap;
  Patch<float> overscan, wheelStep;
};

class VirtualGrid : public detail::KeyedChildren {
  VirtualGridProps _props;

  math::Vec2f _offset;

  math::Size2 _viewport;
  math::Size2 _extent;
  layout::LayoutDirection _direction{};
  std::vector<ItemKey> _visible;
  std::unordered_map<ItemKey, std::size_t> _indices;

  double strideX() const {
    return static_cast<double>(_props.cellExtent.width) + _props.gap.horizontal;
  }
  double strideY() const {
    return static_cast<double>(_props.cellExtent.height) + _props.gap.vertical;
  }
  std::size_t rows() const {
    return _keys.size() / _props.columns + (_keys.size() % _props.columns != 0);
  }
  void rebuildIndex();
  void clampOffset() noexcept;
  struct Anchor {
    ItemKey key;
    math::Vec2f within;
  };
  std::optional<Anchor> anchor() const;
  void restoreAnchor(const std::optional<Anchor> &saved);

protected:
  void prepareChildren(MeasureContext &context,
                       const layout::SizeConstraints &offered) override;
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override {
    return {_viewport};
  }
  void arrangeChildren(ArrangeContext &context, math::Rect content) override;
  void onDefaultEvent(UIEvent &event) override;

public:
  VirtualGrid(std::shared_ptr<const CollectionSource> source,
              ItemFactory factory, VirtualGridProps props = {},
              layout::BoxProps box = {});
  const VirtualGridProps &props() const noexcept { return _props; }
  void applyPatch(const VirtualGridPatch &p);
  void setProps(VirtualGridProps props);
  math::Vec2f offset() const noexcept { return _offset; }
  math::Size2 viewportExtent() const noexcept { return _viewport; }
  math::Size2 contentExtent() const noexcept { return _extent; }
  std::span<const ItemKey> visibleKeys() const noexcept { return _visible; }
  void setOffset(math::Vec2f value);
  void scrollToCell(std::size_t row, std::size_t column);
  void applyChanges(const CollectionChangeSet &batch);
  void applyCollectionChanges();
};
} // namespace playground::ui
