#include <ui/collections/ScrollView.hpp>

namespace playground::ui {

void ScrollView::clampOffset() noexcept {
  _offset.x = horizontal()
                  ? std::clamp(_offset.x, 0.0f,
                               std::max(0.0f, _extent.width - _viewport.width))
                  : 0;
  _offset.y =
      vertical() ? std::clamp(_offset.y, 0.0f,
                              std::max(0.0f, _extent.height - _viewport.height))
                 : 0;
}

bool ScrollView::showBar(layout::Axis axis) const {
  if (_props.scrollbar == ScrollbarPolicy::Never)
    return false;
  const bool x = axis == layout::Axis::Horizontal;
  return (x ? horizontal() : vertical()) &&
         (_props.scrollbar == ScrollbarPolicy::Always ||
          (x ? _extent.width > _viewport.width
             : _extent.height > _viewport.height));
}

math::Rect ScrollView::thumb(layout::Axis axis) const {
  const bool x = axis == layout::Axis::Horizontal;
  const auto content =
      math::inset(math::Rect{{}, bounds().size}, contentInsets());
  const float viewport = x ? _viewport.width : _viewport.height,
              extent = x ? _extent.width : _extent.height,
              offset = x ? _offset.x : _offset.y;
  const float length = std::min(
      viewport, std::max(_props.minimumThumb,
                         extent > 0 ? viewport * viewport / extent : viewport));
  const float start = extent > viewport
                          ? offset / (extent - viewport) * (viewport - length)
                          : 0;
  return x ? math::rect(content.x() + start,
                        content.bottom() -
                            std::min(content.h(), _props.scrollbarThickness),
                        length,
                        std::min(content.h(), _props.scrollbarThickness))
           : math::rect(content.right() -
                            std::min(content.w(), _props.scrollbarThickness),
                        content.y() + start,
                        std::min(content.w(), _props.scrollbarThickness),
                        length);
}

bool ScrollView::hitTestOverlay(math::Point2 point) const {
  for (auto axis : {layout::Axis::Horizontal, layout::Axis::Vertical})
    if (showBar(axis) && thumb(axis).contains(point))
      return true;
  return false;
}

layout::MeasureResult
ScrollView::measureContent(MeasureContext &context,
                           const layout::SizeConstraints &offered) {
  if ((horizontal() && !offered.width.maximum) ||
      (vertical() && !offered.height.maximum))
    throw std::invalid_argument(
        "ScrollView requires finite constraints on scrollable axes");
  layout::SizeConstraints content{
      horizontal() ? layout::AxisConstraints{} : offered.width,
      vertical() ? layout::AxisConstraints{} : offered.height};
  _extent = children().empty() ? math::Size2{}
                               : children()[0]->measure(context, content).size;
  _viewport =
      offered.clamp({horizontal() ? *offered.width.maximum : _extent.width,
                     vertical() ? *offered.height.maximum : _extent.height});
  clampOffset();
  return {_viewport};
}

void ScrollView::arrangeChildren(ArrangeContext &context, math::Rect content) {
  _viewport = content.size;
  clampOffset();
  if (!children().empty())
    children()[0]->arrange(
        context, {{content.x() - _offset.x, content.y() - _offset.y}, _extent});
}

void ScrollView::paintSubtree(PaintContext &context) const {
  Node::paintSubtree(context);
  for (auto axis : {layout::Axis::Horizontal, layout::Axis::Vertical})
    if (showBar(axis))
      context.fill(thumb(axis), _props.scrollbarColor);
}

void ScrollView::onDefaultEvent(UIEvent &event) {
  if (event.type == EventType::PointerCancel ||
      event.type == EventType::FocusLost) {
    _dragPointer.reset();
    releaseAllPointers();
    return;
  }
  if (event.type == EventType::PointerDown && event.button == 1 &&
      !event.handled) {
    for (auto axis : {layout::Axis::Horizontal, layout::Axis::Vertical})
      if (showBar(axis) && thumb(axis).contains(event.localPosition)) {
        _dragPointer = event.pointer;
        _dragAxis = axis;
        _dragStart = axis == layout::Axis::Horizontal ? event.localPosition.x
                                                      : event.localPosition.y;
        _dragOffset = axis == layout::Axis::Horizontal ? _offset.x : _offset.y;
        capturePointer(event.pointer);
        event.handled = true;
        return;
      }
  }
  if (_dragPointer && *_dragPointer == event.pointer) {
    if (event.type == EventType::PointerMove) {
      const bool x = _dragAxis == layout::Axis::Horizontal;
      const auto bar = thumb(_dragAxis);
      const float track =
          (x ? _viewport.width - bar.w() : _viewport.height - bar.h());
      auto offset = _offset;
      if (track > 0)
        (x ? offset.x : offset.y) =
            _dragOffset +
            ((x ? event.localPosition.x : event.localPosition.y) - _dragStart) /
                track *
                (x ? _extent.width - _viewport.width
                   : _extent.height - _viewport.height);
      setOffset(offset);
      event.handled = true;
      return;
    }
    if (event.type == EventType::PointerUp && event.button == 1) {
      releasePointer(event.pointer);
      _dragPointer.reset();
      event.handled = true;
      return;
    }
  }
  if (event.type != EventType::Wheel || event.phase == EventPhase::Capture ||
      event.defaultPrevented)
    return;
  const auto before = _offset;
  scrollBy(event.delta * _props.wheelStep);
  const auto consumed = _offset - before;
  if (consumed != math::Vec2f{}) {
    event.handled = true;
    event.delta -= consumed / _props.wheelStep;
    if (math::almostEqual(event.delta, {}))
      event.stopPropagation();
  }
}

ScrollView::ScrollView(std::unique_ptr<Node> content, ScrollProps props,
                       layout::BoxProps box)
    : Node{box}, _props{props} {
  _props.validate();
  setClip(true);
  setHitTestPolicy(HitTestPolicy::SelfAndChildren);
  if (content)
    appendChild(std::move(content));
}

void ScrollView::setChild(std::unique_ptr<Node> value) {
  if (!value) {
    if (!children().empty())
      takeChildAt(0);
    return;
  }
  appendChild(std::move(value));
  if (children().size() > 1)
    takeChildAt(0);
}

void ScrollView::applyPatch(const ScrollPatch &p) {
  const ScrollProps d;
  setProps(
      {p.axes.appliedTo(_props.axes, d.axes),
       p.wheelStep.appliedTo(_props.wheelStep, d.wheelStep),
       p.scrollbar.appliedTo(_props.scrollbar, d.scrollbar),
       p.scrollbarThickness.appliedTo(_props.scrollbarThickness,
                                      d.scrollbarThickness),
       p.minimumThumb.appliedTo(_props.minimumThumb, d.minimumThumb),
       p.scrollbarColor.appliedTo(_props.scrollbarColor, d.scrollbarColor)});
}

void ScrollView::setProps(ScrollProps props) {
  props.validate();
  _props = props;
  clampOffset();
  invalidateLayout();
}

void ScrollView::setOffset(math::Vec2f value) {
  if (!math::isFinite(value))
    throw std::invalid_argument("Scroll offset must be finite");
  const auto previous = _offset;
  _offset = value;
  clampOffset();
  if (previous != _offset)
    invalidateLayout();
}

void ScrollView::scrollIntoView(math::Rect target,
                                std::optional<layout::Alignment> alignment) {
  if (!math::isFinite(target) || !math::isNonNegative(target.size))
    throw std::invalid_argument("Invalid scroll target");
  auto next = _offset;
  if (alignment) {
    auto align = [](float start, float extent, float viewport,
                    layout::Align value) {
      switch (value) {
      case layout::Align::Start:
        return start;
      case layout::Align::Center:
        return start + (extent - viewport) / 2;
      case layout::Align::End:
        return start + extent - viewport;
      default:
        throw std::invalid_argument(
            "Scroll alignment must be Start, Center, or End");
      }
    };
    setOffset(
        {align(target.x(), target.w(), _viewport.width, alignment->horizontal),
         align(target.y(), target.h(), _viewport.height, alignment->vertical)});
    return;
  }
  if (target.left() < next.x)
    next.x = target.left();
  else if (target.right() > next.x + _viewport.width)
    next.x = target.right() - _viewport.width;
  if (target.top() < next.y)
    next.y = target.top();
  else if (target.bottom() > next.y + _viewport.height)
    next.y = target.bottom() - _viewport.height;
  setOffset(next);
}

void ScrollView::scrollIntoView(const Node &target,
                                std::optional<layout::Alignment> alignment) {
  bool descendant{};
  for (auto *p = target.parent(); p; p = p->parent())
    if (p == this) {
      descendant = true;
      break;
    }
  if (!descendant)
    throw std::invalid_argument("Scroll target is not inside this viewport");
  const auto inverse = worldTransform().inverse();
  if (!inverse)
    throw std::invalid_argument("Scroll viewport transform is singular");
  auto targetBounds = (*inverse * target.worldTransform())
                          .mapBounds({{}, target.bounds().size});
  const auto inset = contentInsets();
  targetBounds.position +=
      math::Vec2f{_offset.x - inset.left, _offset.y - inset.top};
  scrollIntoView(targetBounds, alignment);
}

} // namespace playground::ui
