#pragma once

#include <ui/containers/Box.hpp>

namespace playground::ui {

// Fixed border-box extent; descendants cannot contribute size or baselines.
class LayoutBoundary final : public Box {
protected:
  void validateBoxProps(const layout::BoxProps &box) const override {
    if (box.width.kind() != layout::SizeKind::Fixed ||
        box.height.kind() != layout::SizeKind::Fixed)
      throw std::invalid_argument(
          "LayoutBoundary requires two fixed size rules");
  }

  bool isolatesChildLayout() const noexcept override { return true; }

  layout::MeasureResult
  measureContent(MeasureContext &,
                 const layout::SizeConstraints &offered) override {
    const auto insets = contentInsets();
    return {{offered.width.clamp(
                 std::max(0.0f, extent().width - insets.horizontal())),
             offered.height.clamp(
                 std::max(0.0f, extent().height - insets.vertical()))}};
  }

public:
  LayoutBoundary(math::Size2 extent, std::unique_ptr<Node> child) {
    setExtent(extent);
    setContentAlignment(layout::Alignment::stretch());
    setChild(std::move(child));
  }

  math::Size2 extent() const noexcept {
    return {boxProps().width.value(), boxProps().height.value()};
  }

  void setExtent(math::Size2 extent) {
    if (!math::isFinite(extent) || !math::isNonNegative(extent))
      throw std::invalid_argument(
          "Layout boundary extent must be finite and nonnegative");
    auto box = boxProps();
    box.width = layout::SizeRule::fixed(extent.width);
    box.height = layout::SizeRule::fixed(extent.height);
    setBoxProps(box);
  }
};

class Transform : public Box {
public:
  explicit Transform(std::unique_ptr<Node> child, VisualProps visual = {},
                     layout::BoxProps box = {});
};

struct ClipProps {
  math::CornerRadii cornerRadii;
  bool operator==(const ClipProps &) const = default;
};

struct ClipPatch {
  Patch<math::CornerRadii> cornerRadii;
};

class Clip : public Box {
  ClipProps _clipProps;

public:
  const ClipProps &clipProps() const noexcept { return _clipProps; }

  void setClipProps(ClipProps props);

  void applyPatch(const ClipPatch &patch) {
    setClipProps({patch.cornerRadii.appliedTo(_clipProps.cornerRadii, {})});
  }

  using Box::applyContentPatch;

  const math::CornerRadii &cornerRadii() const noexcept {
    return _clipProps.cornerRadii;
  }

  void setCornerRadii(math::CornerRadii radii) { setClipProps({radii}); }

  void setClipShape(math::ClipShape shape);

  bool containsClip(math::Point2 point) const override {
    return math::RoundedRect{clipBounds(), cornerRadii()}.contains(point);
  }

  void applyContentClip(PaintContext &context) const override;

  math::Rect clipBounds() const noexcept override {
    return visualProps().clipRect.value_or(
        math::inset(math::Rect{{}, bounds().size}, contentInsets()));
  }

  explicit Clip(std::unique_ptr<Node> child,
                std::optional<math::Rect> clip = {}, layout::BoxProps box = {});
  void setClipRect(std::optional<math::Rect> rectangle);
};

enum class LayerCachePolicy { None, WhenUnchanged };

struct LayerProps {
  LayerCachePolicy cachePolicy{LayerCachePolicy::None};
  float rasterScale{1};
  std::size_t byteLimit{16 * 1024 * 1024};
  bool
      useGraphicsScale{}; // content-only layers opt in; protect text by default

  void validate() const {
    if (!std::isfinite(rasterScale) || rasterScale <= 0)
      throw std::invalid_argument("Invalid layer raster scale");
  }

  bool operator==(const LayerProps &) const = default;
};

struct LayerPatch {
  Patch<LayerCachePolicy> cachePolicy;
  Patch<float> rasterScale;
  Patch<std::size_t> byteLimit;
  Patch<bool> useGraphicsScale;
};

class Layer : public Box {
  LayerProps _props;
  float _graphicsScale{1};

  std::weak_ptr<UIServices::CacheBudget> _budget;
  mutable Connection _reservation;

  mutable std::shared_ptr<const rendering::PaintImage> _cache;
  mutable Revision _cachedRevision{};
  mutable rendering::ResourceDomainId _cachedDomain{};
  mutable math::Rect _cachedBounds;
  mutable math::Vec2f _cachedScale;

protected:
  void onAttach(UIServices &services) override {
    _budget = services.rasterBudget;
  }

  void onDetach() noexcept override {
    dropCache();
    _budget.reset();
  }

  void prepareContent(PrepareContext &context) override {
    const float scale = _props.useGraphicsScale && context.graphics
                            ? context.graphics->requested.twoD.rasterScale
                            : 1;
    if (scale != _graphicsScale) {
      _graphicsScale = scale;
      invalidatePaint();
    }
    context.pixelScale *= _props.rasterScale * _graphicsScale;
  }

  void paintSubtree(PaintContext &context) const override;

public:
  explicit Layer(std::unique_ptr<Node> child, LayerProps props = {},
                 layout::BoxProps box = {});

  const LayerProps &props() const noexcept { return _props; }

  void setProps(LayerProps value);
  void applyPatch(const LayerPatch &p);

  void dropCache() const noexcept {
    _cache.reset();
    _reservation.disconnect();
  }

  std::size_t estimatedCacheBytes() const noexcept;
};

} // namespace playground::ui
