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
  float gap{4}, viewportPadding{8}, maximumHeight{320};
  bool dismissOutside{true}, dismissOnEscape{true}, closeOnTab{true};
  bool autoFocus{true};
  std::optional<math::ColorRGBA8> backdrop;
  bool operator==(const PopupProps &) const = default;
};

struct PopupPatch {
  Patch<bool> open;
  Patch<NodeId> anchor;
  Patch<PopupPlacement> placement;
  Patch<std::vector<PopupPlacement>> fallbacks;
  Patch<PopupWidth> width;
  Patch<float> gap, viewportPadding, maximumHeight;
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
};

class Popup : public Portal {
  PopupProps _props;
  Signal<DismissReason> _dismissed;

public:
  explicit Popup(std::unique_ptr<Node>, PopupProps = {});

  const PopupProps &popupProps() const noexcept override { return _props; }

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
