#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <ui/containers/Popup.hpp>

namespace playground::ui {
Popup::Popup(std::unique_ptr<Node> content, PopupProps props) {
  if (!content)
    throw std::invalid_argument("Popup requires content");
  setChild(std::move(content));
  setContentAlignment(layout::Alignment::stretch());
  setClip(true);
  setPopupProps(std::move(props));
}

void Popup::setPopupProps(PopupProps value) {
  if (value.position && !math::isFinite(*value.position))
    throw std::invalid_argument("Invalid popup position");
  if ((value.gap && (!std::isfinite(*value.gap) || *value.gap < 0)) ||
      (value.viewportPadding && (!std::isfinite(*value.viewportPadding) ||
                                 *value.viewportPadding < 0)) ||
      (value.maximumHeight &&
       (!std::isfinite(*value.maximumHeight) || *value.maximumHeight <= 0)) ||
      value.placement < PopupPlacement::BelowStart ||
      value.placement > PopupPlacement::Center ||
      value.width < PopupWidth::Content ||
      value.width > PopupWidth::MatchAnchor)
    throw std::invalid_argument("Invalid popup placement");
  for (auto p : value.fallbacks)
    if (p < PopupPlacement::BelowStart || p > PopupPlacement::Center)
      throw std::invalid_argument("Invalid popup fallback");
  _props = std::move(value);
  setVisibility(_props.open ? Visibility::Visible : Visibility::Collapsed);
  invalidateLayout();
}

void Popup::applyPopupPatch(const PopupPatch &p) {
  const PopupProps d;
  setPopupProps(
      {p.open.appliedTo(_props.open, d.open),
       p.anchor.appliedTo(_props.anchor, d.anchor),
       p.placement.appliedTo(_props.placement, d.placement),
       p.fallbacks.appliedTo(_props.fallbacks, d.fallbacks),
       p.width.appliedTo(_props.width, d.width),
       p.gap.appliedTo(_props.gap, d.gap),
       p.viewportPadding.appliedTo(_props.viewportPadding, d.viewportPadding),
       p.maximumHeight.appliedTo(_props.maximumHeight, d.maximumHeight),
       p.dismissOutside.appliedTo(_props.dismissOutside, d.dismissOutside),
       p.dismissOnEscape.appliedTo(_props.dismissOnEscape, d.dismissOnEscape),
       p.closeOnTab.appliedTo(_props.closeOnTab, d.closeOnTab),
       p.autoFocus.appliedTo(_props.autoFocus, d.autoFocus),
       p.backdrop.appliedTo(_props.backdrop, d.backdrop), _props.position,
       _props.themeBackdrop});
}

void Popup::setOpen(bool open) {
  if (_props.open == open)
    return;
  auto p = _props;
  p.open = open;
  setPopupProps(std::move(p));
}

void Popup::setAnchor(NodeId anchor) {
  if (_props.anchor == anchor)
    return;
  auto p = _props;
  p.anchor = anchor;
  setPopupProps(std::move(p));
}

void Popup::dismiss(DismissReason reason) {
  if (!_props.open)
    return;
  setOpen(false);
  _dismissed.emit(reason);
}

void Popup::present(ArrangeContext &context, math::Rect viewport,
                    Node *anchor) {
  const auto effective = effectivePopupProps();
  const auto inset = std::min(*effective.viewportPadding,
                              std::min(viewport.w(), viewport.h()) / 2);
  const auto usable = math::inset(viewport, math::Insets::all(inset));
  const auto anchorBounds =
      anchor ? anchor->worldTransform().mapBounds({{}, anchor->bounds().size})
             : usable;
  const auto target =
      _props.position ? math::rect(_props.position->x, _props.position->y, 0, 0)
                      : anchorBounds;
  const float width = _props.width == PopupWidth::MatchAnchor && anchor
                          ? std::min(anchorBounds.w(), usable.w())
                          : usable.w();
  const auto offeredWidth = _props.width == PopupWidth::MatchAnchor && anchor
                                ? layout::AxisConstraints::tight(width)
                                : layout::AxisConstraints{0, width};
  auto desired =
      measure(context, {offeredWidth,
                        {0, std::min(usable.h(), *effective.maximumHeight)}})
          .size;
  desired.width = std::min(desired.width, width);
  desired.height =
      std::min({desired.height, usable.h(), *effective.maximumHeight});
  if (_props.width == PopupWidth::MatchAnchor && anchor)
    desired.width = width;
  const auto place = [&](PopupPlacement p) {
    if (p == PopupPlacement::Center)
      return math::rect(usable.x() + (usable.w() - desired.width) / 2,
                        usable.y() + (usable.h() - desired.height) / 2,
                        desired.width, desired.height);
    const bool above =
        p == PopupPlacement::AboveStart || p == PopupPlacement::AboveEnd;
    const bool end =
        (p == PopupPlacement::AboveEnd || p == PopupPlacement::BelowEnd) !=
        (context.direction == layout::LayoutDirection::RightToLeft);
    return math::rect(end ? target.right() - desired.width : target.x(),
                      above ? target.y() - *effective.gap - desired.height
                            : target.bottom() + *effective.gap,
                      desired.width, desired.height);
  };
  auto bounds = place(_props.placement);
  const auto fits = [&](math::Rect r) {
    return r.x() >= usable.x() && r.y() >= usable.y() &&
           r.right() <= usable.right() && r.bottom() <= usable.bottom();
  };
  if (!fits(bounds))
    for (auto p : _props.fallbacks) {
      auto next = place(p);
      if (fits(next)) {
        bounds = next;
        break;
      }
    }
  bounds.position.x =
      std::clamp(bounds.x(), usable.x(),
                 std::max(usable.x(), usable.right() - bounds.w()));
  bounds.position.y =
      std::clamp(bounds.y(), usable.y(),
                 std::max(usable.y(), usable.bottom() - bounds.h()));
  arrange(context, bounds);
}

void Popup::paint(PaintContext &context) const {
  context.fill({{}, bounds().size}, theme().elevated);
}
} // namespace playground::ui
