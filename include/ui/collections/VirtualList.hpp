#pragma once

#include <ui/collections/Collection.hpp>
#include <ui/collections/ExtentIndex.hpp>

namespace playground::ui {

enum class ItemExtentMode { Fixed, Estimated };

struct VirtualListProps {
  layout::Axis axis{layout::Axis::Vertical};
  ItemExtentMode extentMode{ItemExtentMode::Fixed};
  float itemExtent{32};
  float gap{};
  float overscan{64};
  float wheelStep{32};
  bool operator==(const VirtualListProps &) const = default;

  void validate() const {
    if (!std::isfinite(itemExtent) || itemExtent <= 0)
      throw std::invalid_argument(
          "Virtual list item extent must be positive and finite");
    layout::detail::nonnegative(gap, "Virtual list gap must be nonnegative");
    layout::detail::nonnegative(overscan,
                                "Virtual list overscan must be nonnegative");
    layout::detail::nonnegative(wheelStep,
                                "Virtual list wheel step must be nonnegative");
  }
};

struct VirtualListPatch {
  Patch<layout::Axis> axis;
  Patch<ItemExtentMode> extentMode;
  Patch<float> itemExtent, gap, overscan, wheelStep;
};

class VirtualList : public detail::KeyedChildren {
  VirtualListProps _props;

  float _offset{};

  detail::ExtentIndex _extents;
  std::unordered_map<ItemKey, std::size_t> _indices;
  std::unordered_map<ItemKey, float> _measuredExtents;
  math::Size2 _viewport;
  std::vector<ItemKey> _visible;
  std::optional<float> _measuredCross;
  std::uint64_t _measuredEnvironment{};
  layout::LayoutDirection _measuredDirection{};
  math::Vec2f _measuredScale{1, 1};

  float main(math::Size2 size) const {
    return _props.axis == layout::Axis::Vertical ? size.height : size.width;
  }

  float cross(math::Size2 size) const {
    return _props.axis == layout::Axis::Vertical ? size.width : size.height;
  }

  float extent() const {
    return layout::detail::checked(
        std::max(0.0, _extents.total() - (_keys.empty() ? 0 : _props.gap)));
  }

  void clampOffset() {
    _offset =
        std::clamp(_offset, 0.0f, std::max(0.0f, extent() - main(_viewport)));
  }

  struct Anchor {
    ItemKey key;
    float within;
  };

  std::optional<Anchor> anchor() const;
  void restoreAnchor(const std::optional<Anchor> &value);
  void rebuildIndex();
  std::vector<ItemKey> visibleRange() const;

protected:
  void prepareChildren(MeasureContext &context,
                       const layout::SizeConstraints &offered) override;
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void arrangeChildren(ArrangeContext &context, math::Rect content) override;
  void onDefaultEvent(UIEvent &event) override;

public:
  VirtualList(std::shared_ptr<const CollectionSource> source,
              ItemFactory factory, VirtualListProps props = {},
              layout::BoxProps box = {});

  const VirtualListProps &props() const noexcept { return _props; }

  void applyPatch(const VirtualListPatch &p);
  void setProps(VirtualListProps props);

  float offset() const noexcept { return _offset; }

  float contentExtent() const { return extent(); }

  math::Size2 viewportExtent() const noexcept { return _viewport; }

  std::span<const ItemKey> visibleKeys() const noexcept { return _visible; }

  void setOffset(float value);
  void scrollToKey(const ItemKey &key);
  void refreshItem(const ItemKey &key) override;
  void applyChanges(const CollectionChangeSet &batch);
  void applyCollectionChanges();
};
} // namespace playground::ui
