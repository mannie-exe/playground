#pragma once

#include <optional>
#include <vector>

#include <ui/containers/Box.hpp>

namespace playground::ui {
enum class PopupPlacement {
  BelowStart,
  AboveStart,
  BelowEnd,
  AboveEnd,
  Center
};
enum class PopupWidth { Content, MatchAnchor };
enum class DismissReason { Escape, OutsidePointer, Tab, OwnerUnavailable };

struct PopupProps {
  bool open{};
  NodeId anchor;
  PopupPlacement placement{PopupPlacement::BelowStart};
  std::vector<PopupPlacement> fallbacks{PopupPlacement::AboveStart};
  PopupWidth width{PopupWidth::MatchAnchor};
  std::optional<float> gap, viewportPadding, maximumHeight;
  bool dismissOutside{true}, dismissOnEscape{true}, closeOnTab{true};
  bool autoFocus{true};
  std::optional<math::ColorRGBA8> backdrop;
  std::optional<math::Point2> position;
  bool themeBackdrop{};
  bool operator==(const PopupProps &) const = default;
};

struct PopupPatch {
  Patch<bool> open;
  Patch<NodeId> anchor;
  Patch<PopupPlacement> placement;
  Patch<std::vector<PopupPlacement>> fallbacks;
  Patch<PopupWidth> width;
  Patch<std::optional<float>> gap, viewportPadding, maximumHeight;
  Patch<bool> dismissOutside, dismissOnEscape, closeOnTab, autoFocus;
  Patch<std::optional<math::ColorRGBA8>> backdrop;
};

// Logical owner retains this node; UIRoot is the sole presentation host.
class Portal : public Box {
public:
  using Box::Box;

  bool isPortal() const noexcept override { return true; }

  virtual void present(ArrangeContext &, math::Rect viewport, Node *anchor) = 0;
  virtual void dismiss(DismissReason) = 0;
  virtual const PopupProps &popupProps() const noexcept = 0;

  std::optional<math::ColorRGBA8> resolvedBackdrop() const {
    const auto &p = popupProps();
    if (!p.themeBackdrop && !p.backdrop)
      return {};
    return resolveColor(&ThemePalette::backdrop, p.backdrop);
  }
};

class Popup : public Portal {
  PopupProps _props;
  Signal<DismissReason> _dismissed;

public:
  explicit Popup(std::unique_ptr<Node>, PopupProps = {});

  const PopupProps &popupProps() const noexcept override { return _props; }

  PopupProps effectivePopupProps() const {
    auto p = _props;
    p.gap = p.gap.value_or(themeMetrics().popupGap);
    p.viewportPadding = p.viewportPadding.value_or(themeMetrics().popupPadding);
    p.maximumHeight =
        p.maximumHeight.value_or(themeMetrics().popupMaximumHeight);
    p.backdrop = resolvedBackdrop();
    return p;
  }

  void setPopupProps(PopupProps);
  void applyPopupPatch(const PopupPatch &);
  void setOpen(bool);
  void setAnchor(NodeId);
  void present(ArrangeContext &, math::Rect, Node *) override;
  void dismiss(DismissReason) override;

  Connection
  onDismissed(support::MoveOnlyFunction<void(DismissReason)> callback) {
    return _dismissed.connect(std::move(callback));
  }

protected:
  void paint(PaintContext &) const override;
};
} // namespace playground::ui
