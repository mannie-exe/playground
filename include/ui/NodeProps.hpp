#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include <layout/LayoutPrimitives.hpp>
#include <math/Color.hpp>
#include <support/Patch.hpp>
#include <ui/NodeIdentity.hpp>

namespace playground::ui {

using Revision = std::uint64_t;
using ItemKey = std::string;

enum class Visibility { Visible, Hidden, Collapsed };
enum class HitTestPolicy { None, ChildrenOnly, Self, SelfAndChildren };
enum class SemanticRole {
  None,
  Group,
  Button,
  Text,
  Image,
  ScrollArea,
  Checkbox,
  Switch,
  Radio,
  RadioGroup,
  Slider,
  SpinButton,
  TextField,
  TextArea,
  Password,
  ListBox,
  Option,
  Select,
  Disclosure,
  Tabs,
  Tab,
  Dialog,
  Menu,
  MenuItem,
  Tooltip,
  Progress,
  Meter,
  Toolbar,
  AlertDialog,
  Status
};
enum class SemanticExposure { Auto, Self, ChildrenOnly, HiddenSubtree };
enum class DirtyFlags : unsigned {
  None = 0,
  Measure = 1,
  Arrange = 2,
  Paint = 4,
  HitTest = 8,
  Semantics = 16,
  All = 31
};

constexpr DirtyFlags operator|(DirtyFlags a, DirtyFlags b) {
  return static_cast<DirtyFlags>(static_cast<unsigned>(a) |
                                 static_cast<unsigned>(b));
}

constexpr DirtyFlags operator&(DirtyFlags a, DirtyFlags b) {
  return static_cast<DirtyFlags>(static_cast<unsigned>(a) &
                                 static_cast<unsigned>(b));
}

constexpr bool any(DirtyFlags flags) { return flags != DirtyFlags::None; }

struct ChangeSet {
  DirtyFlags flags{DirtyFlags::None};
  Revision revision{};
};

struct NodeProps {
  Visibility visibility{Visibility::Visible};
  std::optional<std::string> debugName;
  std::optional<ItemKey> key;
  bool operator==(const NodeProps &) const = default;
};

struct NodePatch {
  Patch<Visibility> visibility;
  Patch<std::optional<std::string>> debugName;
  Patch<std::optional<ItemKey>> key;
};

struct PaintStyle {
  std::optional<math::ColorRGBA8> background;
  std::optional<math::ColorRGBA8> borderColor;
  float opacity{1};
  bool themeBackground{};

  void validate() const {
    if (!std::isfinite(opacity) || opacity < 0 || opacity > 1)
      throw std::invalid_argument("Opacity must be in [0,1]");
  }

  bool operator==(const PaintStyle &) const = default;
};

struct PaintStylePatch {
  Patch<std::optional<math::ColorRGBA8>> background;
  Patch<std::optional<math::ColorRGBA8>> borderColor;
  Patch<float> opacity;
  Patch<bool> themeBackground;
};

struct VisualProps {
  math::Transform2D transform;
  math::Point2 pivot{0.5f, 0.5f};
  layout::OverflowPolicy overflow{layout::OverflowPolicy::Visible};
  std::optional<math::Rect> clipRect;

  void validate() const {
    for (float value : {transform.a, transform.b, transform.c, transform.d,
                        transform.tx, transform.ty, pivot.x, pivot.y})
      if (!std::isfinite(value))
        throw std::invalid_argument("Visual transform must be finite");
    if (clipRect &&
        (!math::isFinite(*clipRect) || clipRect->w() < 0 || clipRect->h() < 0))
      throw std::invalid_argument("Invalid clip rectangle");
  }

  bool operator==(const VisualProps &) const = default;
};

struct VisualPatch {
  Patch<math::Transform2D> transform;
  Patch<math::Point2> pivot;
  Patch<layout::OverflowPolicy> overflow;
  Patch<std::optional<math::Rect>> clipRect;
};

struct FocusNeighbors {
  std::optional<NodeId> next, previous, left, right, up, down;
  bool operator==(const FocusNeighbors &) const = default;
};

struct InputProps {
  HitTestPolicy hitTest{HitTestPolicy::ChildrenOnly};
  bool focusable{};
  bool focusScope{};
  bool modal{};
  bool wrapNavigation{true};
  FocusNeighbors neighbors;
  std::optional<NodeId> initialFocus, returnFocus;
  bool operator==(const InputProps &) const = default;
};

struct InputPatch {
  Patch<HitTestPolicy> hitTest;
  Patch<bool> focusable;
  Patch<bool> focusScope, modal, wrapNavigation;
  Patch<FocusNeighbors> neighbors;
  Patch<std::optional<NodeId>> initialFocus, returnFocus;
};

struct SemanticProps {
  SemanticRole role{SemanticRole::None};
  std::string name;
  std::string description;
  std::optional<std::string> value;
  bool enabled{true};
  SemanticExposure exposure{SemanticExposure::Auto};
  std::optional<NodeId> labelledBy;
  std::optional<NodeId> describedBy;
  bool operator==(const SemanticProps &) const = default;
};

struct SemanticPatch {
  Patch<SemanticRole> role;
  Patch<std::string> name;
  Patch<std::string> description;
  Patch<std::optional<std::string>> value;
  Patch<bool> enabled;
  Patch<SemanticExposure> exposure;
  Patch<std::optional<NodeId>> labelledBy, describedBy;
};

struct NodeSettings {
  NodeProps node;
  layout::BoxProps box;
  PaintStyle paint;
  VisualProps visual;
  InputProps input;
  SemanticProps semantics;

  void validate() const {
    box.validate();
    paint.validate();
    visual.validate();
  }

  bool operator==(const NodeSettings &) const = default;
};

struct NodeSettingsPatch {
  NodePatch node;
  layout::BoxPatch box;
  PaintStylePatch paint;
  VisualPatch visual;
  InputPatch input;
  SemanticPatch semantics;
};

inline NodeSettings patched(const NodeSettings &p, const NodeSettingsPatch &v) {
  const NodeSettings defaults;
  NodeSettings result{
      {v.node.visibility.appliedTo(p.node.visibility, defaults.node.visibility),
       v.node.debugName.appliedTo(p.node.debugName, defaults.node.debugName),
       v.node.key.appliedTo(p.node.key, defaults.node.key)},
      layout::patched(p.box, v.box),
      {v.paint.background.appliedTo(p.paint.background,
                                    defaults.paint.background),
       v.paint.borderColor.appliedTo(p.paint.borderColor,
                                     defaults.paint.borderColor),
       v.paint.opacity.appliedTo(p.paint.opacity, defaults.paint.opacity),
       v.paint.themeBackground.appliedTo(p.paint.themeBackground,
                                         defaults.paint.themeBackground)},
      {v.visual.transform.appliedTo(p.visual.transform,
                                    defaults.visual.transform),
       v.visual.pivot.appliedTo(p.visual.pivot, defaults.visual.pivot),
       v.visual.overflow.appliedTo(p.visual.overflow, defaults.visual.overflow),
       v.visual.clipRect.appliedTo(p.visual.clipRect,
                                   defaults.visual.clipRect)},
      {v.input.hitTest.appliedTo(p.input.hitTest, defaults.input.hitTest),
       v.input.focusable.appliedTo(p.input.focusable,
                                   defaults.input.focusable)},
      {v.semantics.role.appliedTo(p.semantics.role, defaults.semantics.role),
       v.semantics.name.appliedTo(p.semantics.name, defaults.semantics.name),
       v.semantics.description.appliedTo(p.semantics.description,
                                         defaults.semantics.description),
       v.semantics.value.appliedTo(p.semantics.value, defaults.semantics.value),
       v.semantics.enabled.appliedTo(p.semantics.enabled,
                                     defaults.semantics.enabled)}};
  result.validate();
  result.input.focusScope =
      v.input.focusScope.appliedTo(p.input.focusScope, false);
  result.input.modal = v.input.modal.appliedTo(p.input.modal, false);
  result.input.wrapNavigation =
      v.input.wrapNavigation.appliedTo(p.input.wrapNavigation, true);
  result.input.neighbors = v.input.neighbors.appliedTo(p.input.neighbors, {});
  result.input.initialFocus =
      v.input.initialFocus.appliedTo(p.input.initialFocus, {});
  result.input.returnFocus =
      v.input.returnFocus.appliedTo(p.input.returnFocus, {});
  result.semantics.exposure = v.semantics.exposure.appliedTo(
      p.semantics.exposure, SemanticExposure::Auto);
  result.semantics.labelledBy =
      v.semantics.labelledBy.appliedTo(p.semantics.labelledBy, {});
  result.semantics.describedBy =
      v.semantics.describedBy.appliedTo(p.semantics.describedBy, {});
  return result;
}

struct LayoutEnvironment {
  math::Size2 viewport;
  math::Vec2f pixelScale{1, 1};
  layout::LayoutDirection direction{layout::LayoutDirection::LeftToRight};
  math::Insets usableInsets;
  Revision revision{};

  void validate() const {
    layout::SizeConstraints::tight(viewport);
    if (!math::isFinite(pixelScale) || pixelScale.x <= 0 || pixelScale.y <= 0)
      throw std::invalid_argument("Display scale must be finite and positive");
    layout::detail::insets(usableInsets);
  }

  bool operator==(const LayoutEnvironment &) const = default;
};

} // namespace playground::ui
