#include <ui/Node.hpp>

namespace playground::ui {

void Node::attach(const std::shared_ptr<detail::NodeTable> &table) {
  _table = table;
  const std::size_t index =
      table->freeHead == std::numeric_limits<std::uint32_t>::max()
          ? table->slots.size()
          : table->freeHead;
  if (index >= std::numeric_limits<std::uint32_t>::max())
    throw std::length_error("UI node handle table exhausted");
  if (index == table->slots.size())
    table->slots.push_back({});
  auto &slot = table->slots[index];
  if (index == table->freeHead)
    table->freeHead = slot.nextFree;
  slot.node = this;
  _id = {static_cast<std::uint32_t>(index), slot.generation};
  try {
    if (table->services) {
      LifecycleScope scope{*table};
      onAttach(*table->services);
    }
    for (auto &child : _children)
      child->attach(table);
    invalidate(DirtyFlags::All);
    if (table->stats)
      ++table->stats->realized;
  } catch (...) {
    detach();
    throw;
  }
}

void Node::detach() noexcept {
  for (auto &child : _children)
    child->detach();
  if (auto table = _table.lock(); table && table->resolve(_id) == this) {
    {
      LifecycleScope scope{*table};
      onDetach();
    }
    if (table->focused == _id)
      table->focused = {};
    std::erase_if(table->captures,
                  [this](const auto &capture) { return capture.node == _id; });
    auto &slot = table->slots[_id.index];
    slot.node = nullptr;
    if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
      ++slot.generation;
      if (slot.generation != std::numeric_limits<std::uint64_t>::max()) {
        slot.nextFree = table->freeHead;
        table->freeHead = _id.index;
      }
    }
  }
  _table.reset();
  _id = {};
  _queued = false;
}

std::optional<float> Node::requested(const layout::SizeRule &rule,
                                     const layout::AxisConstraints &offered) {
  using Kind = layout::SizeKind;
  switch (rule.kind()) {
  case Kind::Fixed:
    return rule.value();
  case Kind::Percent:
    if (offered.maximum)
      return *offered.maximum * rule.value();
    return std::nullopt;
  case Kind::Fill:
    return offered.maximum;
  default:
    return std::nullopt;
  }
}

void Node::paintSubtree(PaintContext &context) const {
  paint(context);
  for (const auto &child : _children)
    child->render(context);
}

void Node::checkStructuralMutation() const {
  if (auto table = _table.lock();
      table &&
      (table->lifecycleCallbacks || (table->traversals && !_preparingChildren)))
    throw std::logic_error(
        "Defer structural UI changes until traversal has completed");
}

Node &Node::insertChildAt(std::size_t index, std::unique_ptr<Node> child) {
  checkStructuralMutation();
  if (index > _children.size())
    throw std::out_of_range("UI child insertion index");
  if (!child || child->_parent || !child->_table.expired())
    throw std::invalid_argument("UI child must be non-null and detached");
  _children.reserve(_children.size() + 1);
  child->_parent = this;
  try {
    if (auto table = _table.lock())
      child->attach(table);
  } catch (...) {
    child->_parent = nullptr;
    throw;
  }
  _children.insert(_children.begin() + static_cast<std::ptrdiff_t>(index),
                   std::move(child));
  invalidateLayout();
  return *_children[index];
}

void Node::moveChildAt(std::size_t from, std::size_t to) {
  checkStructuralMutation();
  if (from >= _children.size() || to >= _children.size())
    throw std::out_of_range("UI child reorder index");
  if (from < to)
    std::rotate(_children.begin() + from, _children.begin() + from + 1,
                _children.begin() + to + 1);
  else if (to < from)
    std::rotate(_children.begin() + to, _children.begin() + from,
                _children.begin() + from + 1);
  invalidateLayout();
}

Node &Node::transferChildAt(Node &source, std::size_t from, std::size_t to) {
  checkStructuralMutation();
  source.checkStructuralMutation();
  if (from >= source._children.size() || to > _children.size())
    throw std::out_of_range("UI reparent index");
  if (&source == this)
    throw std::invalid_argument("Use moveChildAt to reorder within one parent");
  Node *child = source._children[from].get();
  for (auto *ancestor = this; ancestor; ancestor = ancestor->_parent)
    if (ancestor == child)
      throw std::invalid_argument("UI reparent would create a cycle");
  if (_table.lock() != source._table.lock())
    throw std::invalid_argument(
        "Cross-root transfer requires detach and attach");
  _children.reserve(_children.size() + 1);
  auto owned = std::move(source._children[from]);
  source._children.erase(source._children.begin() + from);
  owned->_parent = this;
  _children.insert(_children.begin() + to, std::move(owned));
  source.invalidateLayout();
  invalidateLayout();
  return *child;
}

std::unique_ptr<Node> Node::takeChildAt(std::size_t index) {
  checkStructuralMutation();
  if (index >= _children.size())
    throw std::out_of_range("UI child index");
  auto child = std::move(_children[index]);
  _children.erase(_children.begin() + static_cast<std::ptrdiff_t>(index));
  child->detach();
  child->_parent = nullptr;
  invalidateLayout();
  return child;
}

void Node::setProps(NodeSettings value) {
  value.validate();
  DirtyFlags changes = DirtyFlags::None;
  if (value.box != _box || ((value.node.visibility == Visibility::Collapsed) !=
                            (_nodeProps.visibility == Visibility::Collapsed)))
    changes = changes | DirtyFlags::Measure | DirtyFlags::Arrange |
              DirtyFlags::Paint | DirtyFlags::HitTest;
  if (value.node != _nodeProps)
    changes = changes | DirtyFlags::Paint | DirtyFlags::HitTest |
              DirtyFlags::Semantics;
  if (value.paint != _paintStyle)
    changes = changes | DirtyFlags::Paint;
  if (value.visual != _visualProps)
    changes = changes | DirtyFlags::Paint | DirtyFlags::HitTest;
  if (value.input != _inputProps)
    changes = changes | DirtyFlags::HitTest | DirtyFlags::Semantics;
  if (value.semantics != _semanticProps)
    changes = changes | DirtyFlags::Semantics;
  if (!any(changes))
    return;
  _nodeProps = std::move(value.node);
  _box = value.box;
  _paintStyle = value.paint;
  _visualProps = value.visual;
  _inputProps = value.input;
  _semanticProps = std::move(value.semantics);
  invalidate(changes);
}

void Node::setNodeProps(NodeProps value) {
  auto p = props();
  p.node = std::move(value);
  setProps(std::move(p));
}

void Node::setPaintStyle(PaintStyle value) {
  auto p = props();
  p.paint = value;
  setProps(std::move(p));
}

void Node::setVisualProps(VisualProps value) {
  auto p = props();
  p.visual = value;
  setProps(std::move(p));
}

void Node::setInputProps(InputProps value) {
  auto p = props();
  p.input = value;
  setProps(std::move(p));
}

void Node::setSemanticProps(SemanticProps value) {
  auto p = props();
  p.semantics = std::move(value);
  setProps(std::move(p));
}

void Node::applyNodePatch(const NodePatch &p) {
  NodeSettingsPatch patch;
  patch.node = p;
  applyPatch(patch);
}

void Node::applyPaintPatch(const PaintStylePatch &p) {
  NodeSettingsPatch patch;
  patch.paint = p;
  applyPatch(patch);
}

void Node::applyVisualPatch(const VisualPatch &p) {
  NodeSettingsPatch patch;
  patch.visual = p;
  applyPatch(patch);
}

void Node::applyInputPatch(const InputPatch &p) {
  NodeSettingsPatch patch;
  patch.input = p;
  applyPatch(patch);
}

void Node::applySemanticPatch(const SemanticPatch &p) {
  NodeSettingsPatch patch;
  patch.semantics = p;
  applyPatch(patch);
}

void Node::setBoxProps(layout::BoxProps value) {
  auto p = props();
  p.box = value;
  setProps(std::move(p));
}

void Node::setVisibility(Visibility value) {
  auto p = _nodeProps;
  p.visibility = value;
  setNodeProps(std::move(p));
}

void Node::setHitTestPolicy(HitTestPolicy value) {
  auto p = _inputProps;
  p.hitTest = value;
  setInputProps(p);
}

void Node::setFocusable(bool value) {
  auto p = _inputProps;
  p.focusable = value;
  setInputProps(p);
}

void Node::setClip(bool value) {
  auto p = _visualProps;
  p.overflow =
      value ? layout::OverflowPolicy::Clip : layout::OverflowPolicy::Visible;
  setVisualProps(p);
}

void Node::setBackground(std::optional<math::ColorRGBA8> value) {
  auto p = _paintStyle;
  p.background = value;
  setPaintStyle(p);
}

void Node::invalidate(DirtyFlags flags) noexcept {
  _dirty = _dirty | flags;
  _pendingChanges = _pendingChanges | flags;
  ++_sourceRevision;
  if (any(flags &
          (DirtyFlags::Paint | DirtyFlags::Measure | DirtyFlags::Arrange)))
    for (auto *ancestor = this; ancestor; ancestor = ancestor->_parent)
      ++ancestor->_subtreePaintRevision;
  if (any(flags & DirtyFlags::Measure)) {
    _measurement.reset();
    ++_revision;
  }
  if (auto table = _table.lock()) {
    ++table->revision;
    table->layoutDirty |=
        any(flags & (DirtyFlags::Measure | DirtyFlags::Arrange));
    table->paintDirty |= any(flags & (DirtyFlags::Paint | DirtyFlags::Arrange |
                                      DirtyFlags::Measure));
    if (!_queued) {
      try {
        table->dirty.push_back(_id);
        _queued = true;
      } catch (...) {
        table->dirtyFallback = true;
      }
    }
  }
  if (_parent && any(flags & DirtyFlags::Measure))
    _parent->invalidate(flags);
}

bool Node::hasActiveInputInSubtree() const noexcept {
  if (auto table = _table.lock()) {
    auto within = [this](Node *node) {
      for (; node; node = node->_parent)
        if (node == this)
          return true;
      return false;
    };
    if (within(table->resolve(table->focused)))
      return true;
    for (const auto &capture : table->captures)
      if (within(table->resolve(capture.node)))
        return true;
  }
  return false;
}

void Node::releaseAllPointers() noexcept {
  if (auto table = _table.lock())
    std::erase_if(table->captures,
                  [this](const auto &c) { return c.node == _id; });
}

math::Insets Node::contentInsets() const noexcept {
  return {_box.padding.left + _box.borderWidths.left,
          _box.padding.top + _box.borderWidths.top,
          _box.padding.right + _box.borderWidths.right,
          _box.padding.bottom + _box.borderWidths.bottom};
}

layout::MeasureResult Node::measure(MeasureContext &context,
                                    const layout::SizeConstraints &offered) {
  offered.validate();
  if (visibility() == Visibility::Collapsed)
    return {};
  if (_measurement && _measurement->constraints == offered &&
      _measurement->revision == _revision &&
      _measurement->environment == context.environmentRevision &&
      _measurement->pixelScale == context.pixelScale &&
      _measurement->direction == context.direction) {
    if (context.stats)
      ++context.stats->measureCacheHits;
    return _measurement->result;
  }
  if (context.stats)
    ++context.stats->measured;
  const auto insets = contentInsets();
  const float horizontal =
      layout::detail::checked(static_cast<double>(insets.left) + insets.right);
  const float vertical =
      layout::detail::checked(static_cast<double>(insets.top) + insets.bottom);
  auto constrainAxis = [](layout::AxisConstraints incoming, float minimum,
                          std::optional<float> maximum) {
    // Parent constraints win when authored limits cannot be satisfied.
    return layout::AxisConstraints{
        incoming.clamp(minimum),
        maximum ? std::optional<float>{incoming.clamp(*maximum)}
                : incoming.maximum};
  };
  // Author limits must affect content measurement, not just clamp its final
  // box.
  const layout::SizeConstraints effective{
      constrainAxis(offered.width, _box.minWidth, _box.maxWidth),
      constrainAxis(offered.height, _box.minHeight, _box.maxHeight)};
  auto width = requested(_box.width, offered.width);
  auto height = requested(_box.height, offered.height);
  if (context.diagnostics) {
    for (const auto &[rule, axis] : {std::pair{_box.width, offered.width},
                                     std::pair{_box.height, offered.height}})
      if (!axis.maximum && (rule.kind() == layout::SizeKind::Fill ||
                            rule.kind() == layout::SizeKind::Percent))
        context.diagnostics->report(id(), LayoutPhase::Measure,
                                    rule.kind() == layout::SizeKind::Fill
                                        ? LayoutIssue::UnboundedFill
                                        : LayoutIssue::IndefinitePercent,
                                    "Indefinite sizing falls back to content");
    if ((offered.width.maximum && *offered.width.maximum < _box.minWidth) ||
        (offered.height.maximum && *offered.height.maximum < _box.minHeight))
      context.diagnostics->report(id(), LayoutPhase::Measure,
                                  LayoutIssue::ConstraintViolation,
                                  "Parent maximum is below authored minimum");
    if (_box.aspectRatio && width && height &&
        !math::almostEqual(*width, *height * *_box.aspectRatio))
      context.diagnostics->report(
          id(), LayoutPhase::Measure, LayoutIssue::AspectConflict,
          "Definite width and height override aspect ratio");
  }
  if (_box.aspectRatio) {
    if (width && !height)
      height = *width / *_box.aspectRatio;
    if (height && !width)
      width = *height * *_box.aspectRatio;
  }
  auto contentAxis = [](const layout::AxisConstraints &incoming,
                        std::optional<float> desired, float inset) {
    layout::AxisConstraints result;
    if (desired) {
      const float extent = std::max(0.0f, incoming.clamp(*desired) - inset);
      result = layout::AxisConstraints::tight(extent);
    } else if (incoming.maximum) {
      result.maximum = std::max(0.0f, *incoming.maximum - inset);
    }
    if (!desired)
      result.minimum = std::max(0.0f, incoming.minimum - inset);
    return result;
  };
  layout::SizeConstraints inner{
      contentAxis(effective.width, width, horizontal),
      contentAxis(effective.height, height, vertical)};
  {
    _preparingChildren = true;
    struct Guard {
      bool &flag;
      ~Guard() { flag = false; }
    } guard{_preparingChildren};
    prepareChildren(context, inner);
  }
  const auto measuredRevision = _revision;
  auto result = measureContent(context, inner);
  result.validate();
  if (!std::isfinite(result.size.width) || !std::isfinite(result.size.height) ||
      result.size.width < 0 || result.size.height < 0)
    throw std::logic_error(
        "UI measurement must return finite nonnegative extents");
  float w = width.value_or(layout::detail::checked(
      static_cast<double>(result.size.width) + horizontal));
  float h = height.value_or(layout::detail::checked(
      static_cast<double>(result.size.height) + vertical));
  if (_box.aspectRatio && !width && !height) {
    w = std::max(w, h * *_box.aspectRatio);
    h = w / *_box.aspectRatio;
  }
  w = std::max(_box.minWidth, w);
  h = std::max(_box.minHeight, h);
  if (_box.maxWidth)
    w = std::min(w, *_box.maxWidth);
  if (_box.maxHeight)
    h = std::min(h, *_box.maxHeight);
  result.size = {offered.width.clamp(w), offered.height.clamp(h)};
  if (result.firstBaseline)
    *result.firstBaseline += insets.top;
  if (result.lastBaseline)
    *result.lastBaseline += insets.top;
  for (auto *baseline : {&result.firstBaseline, &result.lastBaseline})
    if (*baseline && (**baseline < 0 || **baseline > result.size.height))
      baseline->reset();
  if (_revision == measuredRevision)
    _measurement = Measurement{
        offered,           _revision,          context.environmentRevision,
        context.direction, context.pixelScale, result};
  return result;
}

void Node::arrange(ArrangeContext &context, math::Rect bounds) {
  if (!std::isfinite(bounds.position.x) || !std::isfinite(bounds.position.y) ||
      !std::isfinite(bounds.size.width) || !std::isfinite(bounds.size.height) ||
      bounds.size.width < 0 || bounds.size.height < 0)
    throw std::invalid_argument("Invalid arranged UI rectangle");
  _bounds = bounds;
  _arranged = false;
  if (context.stats)
    ++context.stats->arranged;
  if (visibility() == Visibility::Collapsed) {
    _arranged = true;
    return;
  }
  const auto insets = contentInsets();
  const math::Rect content{
      {insets.left, insets.top},
      {std::max(0.0f, bounds.size.width - insets.left - insets.right),
       std::max(0.0f, bounds.size.height - insets.top - insets.bottom)}};
  arrangeChildren(context, content);
  _layoutResult = {bounds, content, {{}, bounds.size}};
  for (const auto &child : _children)
    if (child->visibility() != Visibility::Collapsed)
      _layoutResult.overflowBounds =
          math::unite(_layoutResult.overflowBounds,
                      child->localTransform().mapBounds(
                          child->layoutResult().overflowBounds));
  _arranged = true;
  // Placement/environment changes can alter a cached descendant raster even
  // when no authored props changed. Do not schedule another layout pass.
  _dirty = _dirty | DirtyFlags::Paint | DirtyFlags::HitTest;
  for (auto *ancestor = this; ancestor; ancestor = ancestor->_parent)
    ++ancestor->_subtreePaintRevision;
  if (auto table = _table.lock())
    table->paintDirty = true;
}

void Node::prepareSubtree(PrepareContext context,
                          math::Transform2D parentToPixels) {
  if (visibility() != Visibility::Visible || !_arranged)
    return;
  const auto &t = _visualProps.transform;
  if (!t.inverse())
    return;
  auto localToPixels = parentToPixels * t;
  context.pixelScale = {std::hypot(localToPixels.a, localToPixels.b),
                        std::hypot(localToPixels.c, localToPixels.d)};
  if (!math::isFinite(context.pixelScale) || context.pixelScale.x <= 0 ||
      context.pixelScale.y <= 0)
    throw std::invalid_argument("Invalid composed raster scale");
  const auto density = context.pixelScale;
  prepareContent(context);
  if (!math::isFinite(context.pixelScale) || context.pixelScale.x <= 0 ||
      context.pixelScale.y <= 0)
    throw std::invalid_argument("Invalid subtree raster scale");
  // Layer preparation may request extra raster resolution for descendants.
  localToPixels =
      localToPixels * math::Transform2D::scaling(context.pixelScale / density);
  if (context.stats)
    ++context.stats->prepared;
  for (auto &child : _children)
    child->prepareSubtree(context, localToPixels);
}

void Node::prepare(PrepareContext context) {
  if (!math::isFinite(context.pixelScale) || context.pixelScale.x <= 0 ||
      context.pixelScale.y <= 0)
    throw std::invalid_argument("Raster scale must be finite and positive");
  prepareSubtree(context, math::Transform2D::scaling(context.pixelScale));
}

math::Rect Node::paintBounds() const {
  math::Rect result{{}, _bounds.size};
  for (const auto &child : _children)
    if (child->visibility() == Visibility::Visible && child->isArranged())
      result = math::unite(
          result, child->localTransform().mapBounds(child->paintBounds()));
  return clipsContent() ? math::intersect(result, clipBounds()) : result;
}

void Node::render(PaintContext &context) const {
  if (visibility() != Visibility::Visible || !_arranged ||
      _paintStyle.opacity == 0 || !_visualProps.transform.inverse())
    return;
  PaintScope scope{context};
  context.transform(localTransform());
  if (clipsContent())
    applyContentClip(context);
  std::optional<LayerScope> layer;
  if (_paintStyle.opacity < 1)
    layer.emplace(context, paintBounds(), _paintStyle.opacity);
  if (_paintStyle.background)
    context.fill({{}, _bounds.size}, *_paintStyle.background);
  if (_paintStyle.borderColor) {
    const auto b = _box.borderWidths;
    const float w = _bounds.w(), h = _bounds.h();
    context.fill(math::rect(0, 0, w, std::min(h, b.top)),
                 *_paintStyle.borderColor);
    context.fill(math::rect(0, std::max(b.top, h - b.bottom), w,
                            std::min(std::max(0.0f, h - b.top), b.bottom)),
                 *_paintStyle.borderColor);
    const float sideHeight = std::max(0.0f, h - b.top - b.bottom);
    context.fill(math::rect(0, b.top, std::min(w, b.left), sideHeight),
                 *_paintStyle.borderColor);
    context.fill(math::rect(std::max(b.left, w - b.right), b.top,
                            std::min(std::max(0.0f, w - b.left), b.right),
                            sideHeight),
                 *_paintStyle.borderColor);
  }
  paintSubtree(context);
  if (layer)
    layer->finish();
  if (auto table = _table.lock(); table && table->stats)
    ++table->stats->painted;
}

math::Transform2D Node::localTransform() const noexcept {
  return math::Transform2D::translation(math::toVector(_bounds.position)) *
         math::Transform2D::around({_bounds.w() * _visualProps.pivot.x,
                                    _bounds.h() * _visualProps.pivot.y},
                                   _visualProps.transform);
}

void Node::capturePointer(std::uint64_t pointer) {
  if (auto table = _table.lock()) {
    for (auto &capture : table->captures) {
      if (capture.pointer == pointer) {
        capture.node = _id;
        return;
      }
    }
    table->captures.push_back({pointer, _id});
  }
}

void Node::releasePointer(std::uint64_t pointer) noexcept {
  if (auto table = _table.lock())
    std::erase_if(table->captures, [&](const auto &c) {
      return c.pointer == pointer && c.node == _id;
    });
}

void Node::requestFocus() {
  if (isFocusable())
    if (auto table = _table.lock()) {
      if (table->requestFocus)
        table->requestFocus(_id);
      else
        table->focused = _id;
    }
}

} // namespace playground::ui
