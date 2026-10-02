#pragma once

#include <ui/Node.hpp>

namespace playground::ui {
// Prepared content that a composing control can draw with paired semantic ink.
class TintableContent : public Node {
protected:
  explicit TintableContent(layout::BoxProps box = {}) : Node{box} {}

public:
  // Local coordinates only; the caller owns transforms, clips and node chrome.
  virtual void paintTinted(PaintContext &, math::ColorRGBA8) const = 0;
};
} // namespace playground::ui
