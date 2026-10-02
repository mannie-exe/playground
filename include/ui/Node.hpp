#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <layout/LayoutPrimitives.hpp>
#include <rendering/GraphicsSettings.hpp>
#include <rendering/PaintContext.hpp>
#include <ui/UITypes.hpp>

namespace playground::scene {
class SceneRenderer;
}

namespace playground::ui {
using rendering::LayerScope;
using rendering::PaintContext;
using rendering::PaintScope;

struct MeasureContext {
  layout::LayoutDirection direction{layout::LayoutDirection::LeftToRight};
  std::uint64_t environmentRevision{};
  UIWorkStats *stats{};
  math::Vec2f pixelScale{1, 1};
  UIServices *services{};
  LayoutDiagnostics *diagnostics{};
};

using ArrangeContext = MeasureContext;

struct PrepareContext {
  math::Vec2f pixelScale{1, 1};
  UIWorkStats *stats{};
  rendering::ImagePreparer *images{};
  scene::SceneRenderer *scenes{};
  rendering::TextImagePreparer *text{};
  const rendering::ResolvedGraphicsState *graphics{};
};

class UIRoot;

class Node {
  friend class UIRoot;

  struct Measurement {
    layout::SizeConstraints constraints;
    std::uint64_t revision;
    std::uint64_t environment;
    layout::LayoutDirection direction;
    math::Vec2f pixelScale;
    layout::MeasureResult result;
  };

  Node *_parent{};
  std::weak_ptr<detail::NodeTable> _table;
  NodeId _id;
  std::vector<std::unique_ptr<Node>> _children;
  Signal<const ChangeSet &> _changes;

  layout::BoxProps _box;
  NodeProps _nodeProps;
  PaintStyle _paintStyle;
  VisualProps _visualProps;
  InputProps _inputProps;
  SemanticProps _semanticProps;
  ThemeOverrides _theme;
  mutable std::optional<ResolvedTheme> _resolvedTheme;
  ControlLayout _controlLayout{ControlLayout::None};
  ControlStyle _controlStyle;

  std::uint64_t _revision{1};
  std::uint64_t _arrangeRevision{1};
  Revision _sourceRevision{1};
  Revision _subtreePaintRevision{1};
  DirtyFlags _dirty{DirtyFlags::All};
  DirtyFlags _pendingChanges{DirtyFlags::None};
  bool _queued{};
  bool _preparingChildren{};

  std::optional<Measurement> _measurement;
  std::optional<Measurement> _previousMeasurement;
  math::Rect _bounds{};
  layout::LayoutResult _layoutResult;
  bool _arranged{};
  std::uint64_t _arrangedRevision{};
  std::uint64_t _arrangedEnvironment{};
  layout::LayoutDirection _arrangedDirection{};
  math::Vec2f _arrangedPixelScale{};

  struct LifecycleScope {
    detail::NodeTable &table;

    explicit LifecycleScope(detail::NodeTable &value) : table{value} {
      ++table.lifecycleCallbacks;
    }

    ~LifecycleScope() { --table.lifecycleCallbacks; }
  };

  void attach(const std::shared_ptr<detail::NodeTable> &table);

  void detach() noexcept;

  static std::optional<float> requested(const layout::SizeRule &rule,
                                        const layout::AxisConstraints &offered);

protected:
  std::uint64_t measureRevision() const noexcept { return _revision; }

  UIServices *services() const noexcept {
    auto table = _table.lock();
    return table ? table->services : nullptr;
  }

  explicit Node(layout::BoxProps box = {}) : _box{box} {
    layout::validate(_box);
  }

  virtual layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) = 0;

  virtual void arrangeChildren(ArrangeContext &, math::Rect) {}

  virtual void prepareChildren(MeasureContext &,
                               const layout::SizeConstraints &) {}

  virtual void prepareContent(PrepareContext &) {}

  virtual void paint(PaintContext &) const {}

  virtual void paintSubtree(PaintContext &context) const;

  virtual void onEvent(UIEvent &) {}

  virtual void onDefaultEvent(UIEvent &) {}

  virtual bool hitTestOverlay(math::Point2) const { return false; }

  virtual void onAttach(UIServices &) {}

  virtual void onPropsChanged(const ChangeSet &) {}

  virtual void onDetach() noexcept {}

  virtual void onThemeChanged() noexcept {}

  virtual bool isolatesChildLayout() const noexcept { return false; }

  // Old offers are reusable only without constraint-dependent side effects.
  virtual bool canReuseMeasurementOffers() const noexcept { return false; }

  virtual void validateBoxProps(const layout::BoxProps &) const {}

  void checkStructuralMutation() const;

  Node &appendChild(std::unique_ptr<Node> child) {
    return insertChildAt(_children.size(), std::move(child));
  }

  Node &insertChildAt(std::size_t index, std::unique_ptr<Node> child);

  void moveChildAt(std::size_t from, std::size_t to);

  Node &transferChildAt(Node &source, std::size_t from, std::size_t to);

  std::unique_ptr<Node> takeChildAt(std::size_t index);

public:
  virtual bool isPortal() const noexcept { return false; }

  const ResolvedTheme &resolvedTheme() const noexcept;

  const ThemePalette &theme() const noexcept { return resolvedTheme().colors; }

  const ThemeMetrics &themeMetrics() const noexcept {
    return resolvedTheme().metrics;
  }

  const ThemeOverrides &themeOverrides() const noexcept { return _theme; }

  void setThemeOverrides(ThemeOverrides);

  void setTheme(std::optional<ThemePalette> value) {
    auto next = _theme;
    next.colors = std::move(value);
    setThemeOverrides(std::move(next));
  }

  void refreshTheme() noexcept;

  math::ColorRGBA8
  resolveColor(math::ColorRGBA8 ThemePalette::*role,
               std::optional<math::ColorRGBA8> authored = {}) const noexcept {
    return !theme().highContrast && authored ? *authored : theme().*role;
  }

  float resolvedFocusWidth(std::optional<float> authored = {}) const noexcept {
    return std::max(themeMetrics().focusWidth, authored.value_or(0));
  }

  void setControlLayout(ControlLayout);
  void setControlStyle(ControlStyle);

  const ControlStyle &controlStyle() const noexcept { return _controlStyle; }

  ControlStyle resolvedControlStyle() const noexcept {
    return resolveControlStyle(_controlLayout, themeMetrics(), _controlStyle);
  }

  layout::BoxProps effectiveBoxProps() const noexcept;

  virtual ~Node() { detach(); }

  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  Node(Node &&) = delete;
  Node &operator=(Node &&) = delete;

  NodeId id() const noexcept { return _id; }

  Node *parent() const noexcept { return _parent; }

  template <typename T = Node> NodeHandle<T> handle() const {
    return {_table, _id};
  }

  std::span<const std::unique_ptr<Node>> children() const noexcept {
    return _children;
  }

  const layout::BoxProps &boxProps() const noexcept { return _box; }

  const NodeProps &nodeProps() const noexcept { return _nodeProps; }

  const PaintStyle &paintStyle() const noexcept { return _paintStyle; }

  const VisualProps &visualProps() const noexcept { return _visualProps; }

  const InputProps &inputProps() const noexcept { return _inputProps; }

  const SemanticProps &semanticProps() const noexcept { return _semanticProps; }

  bool isEffectivelyEnabled() const noexcept {
    for (const Node *node = this; node; node = node->parent())
      if (!node->isInteractionEnabled())
        return false;
    return true;
  }

  virtual bool isInteractionEnabled() const noexcept {
    return _semanticProps.enabled;
  }

  virtual Node &focusTarget() noexcept { return *this; }

  // Composites may present one sequential entry while retaining pointer,
  // assistive and programmatic focus on their other controls.
  virtual Node *sequentialFocusTarget(Node &descendant) { return &descendant; }

  virtual SemanticState semanticState() const {
    SemanticState state{.description = _semanticProps};
    if (isFocusable())
      state.actions.push_back(SemanticAction::Focus);
    return state;
  }

  virtual ActionResult performAction(const UIAction &, ActionSource) {
    return ActionResult::Unsupported;
  }

  const layout::LayoutResult &layoutResult() const noexcept {
    return _layoutResult;
  }

  Revision sourceRevision() const noexcept { return _sourceRevision; }

  Revision subtreePaintRevision() const noexcept {
    return _subtreePaintRevision;
  }

  DirtyFlags dirtyFlags() const noexcept { return _dirty; }

  Connection
  onChanged(support::MoveOnlyFunction<void(const ChangeSet &)> callback) {
    return _changes.connect(std::move(callback));
  }

  NodeSettings settings() const {
    return {_nodeProps,   _box,        _paintStyle,
            _visualProps, _inputProps, _semanticProps};
  }

  math::Rect bounds() const noexcept { return _bounds; }

  bool isArranged() const noexcept { return _arranged; }

  Visibility visibility() const noexcept { return _nodeProps.visibility; }

  HitTestPolicy hitTestPolicy() const noexcept { return _inputProps.hitTest; }

  bool isFocusable() const noexcept { return _inputProps.focusable; }

  bool clipsContent() const noexcept {
    return _visualProps.overflow == layout::OverflowPolicy::Clip;
  }

  virtual math::Rect clipBounds() const noexcept {
    return _visualProps.clipRect.value_or(math::Rect{{}, _bounds.size});
  }

  virtual bool containsLocal(math::Point2 point) const {
    return math::Rect{{}, _bounds.size}.contains(point);
  }

  virtual bool containsClip(math::Point2 point) const {
    return clipBounds().contains(point);
  }

  virtual void applyContentClip(PaintContext &context) const {
    context.clip(clipBounds());
  }

  void setSettings(NodeSettings value);

  void applySettingsPatch(const NodeSettingsPatch &patch) {
    setSettings(patched(settings(), patch));
  }

  void setNodeProps(NodeProps value);
  void setPaintStyle(PaintStyle value);
  void setVisualProps(VisualProps value);
  void setInputProps(InputProps value);
  void setSemanticProps(SemanticProps value);
  void applyNodePatch(const NodePatch &p);
  void applyPaintPatch(const PaintStylePatch &p);
  void applyVisualPatch(const VisualPatch &p);
  void applyInputPatch(const InputPatch &p);
  void applySemanticPatch(const SemanticPatch &p);

  void setBoxProps(layout::BoxProps value);

  void applyBoxPatch(const layout::BoxPatch &patch) {
    setBoxProps(layout::patched(_box, patch));
  }

  void setVisibility(Visibility value);
  void setHitTestPolicy(HitTestPolicy value);
  void setFocusable(bool value);
  void setClip(bool value);
  void setBackground(std::optional<math::ColorRGBA8> value);

  void invalidate(DirtyFlags flags) noexcept;

  void invalidateLayout() noexcept {
    invalidate(DirtyFlags::Measure | DirtyFlags::Arrange | DirtyFlags::Paint |
               DirtyFlags::HitTest);
  }

  // Reposition children within unchanged measured bounds. Size-affecting
  // changes must use invalidateLayout(). Ancestor raster/hit state still
  // changes.
  void invalidateArrange() noexcept;

  void invalidatePaint() noexcept { invalidate(DirtyFlags::Paint); }

  bool hasActiveInputInSubtree() const noexcept;

  bool hasFocus() const noexcept {
    auto table = _table.lock();
    return table && table->focused == _id;
  }

  void clearFocus() noexcept {
    if (auto table = _table.lock(); table && table->focused == _id)
      table->focused = {};
  }

  void releaseAllPointers() noexcept;

  math::Insets contentInsets() const noexcept;

  layout::MeasureResult measure(MeasureContext &context,
                                const layout::SizeConstraints &offered);

  void arrange(ArrangeContext &context, math::Rect bounds);

private:
  void refreshOverflow();
  void prepareSubtree(PrepareContext context, math::Transform2D parentToPixels);

public:
  void prepare(PrepareContext context);

  math::Rect paintBounds() const;

  void render(PaintContext &context, bool overlayPresentation = false) const;

  math::Transform2D localTransform() const noexcept;

  math::Transform2D worldTransform() const noexcept {
    return _parent && !isPortal() ? _parent->worldTransform() * localTransform()
                                  : localTransform();
  }

  math::Point2 originInRoot() const noexcept {
    return worldTransform().mapPoint({});
  }

  void capturePointer(std::uint64_t pointer);
  void releasePointer(std::uint64_t pointer) noexcept;
  void requestFocus();

  void requestFocusAfterLayout() {
    if (auto *s = services(); s && s->focusAfterLayout)
      s->focusAfterLayout(id());
    else
      requestFocus();
  }

  layout::LayoutDirection layoutDirection() const noexcept {
    return _arrangedDirection;
  }
};

} // namespace playground::ui
