#pragma once
#include <format>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include <rendering/RenderFormatters.hpp>
#include <ui/UITypes.hpp>
#include <ui/collections/Repeat.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/collections/VirtualList.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/content/Text.hpp>

namespace playground::ui {

inline std::string toString(DirtyFlags value) {
  if (value == DirtyFlags::None)
    return "None";
  if (value == DirtyFlags::All)
    return "All";
  std::string result;
  for (auto [flag, name] : {std::pair{DirtyFlags::Measure, "Measure"},
                            {DirtyFlags::Arrange, "Arrange"},
                            {DirtyFlags::Paint, "Paint"},
                            {DirtyFlags::HitTest, "HitTest"},
                            {DirtyFlags::Semantics, "Semantics"}}) {
    if (any(value & flag)) {
      if (!result.empty())
        result += '|';
      result += name;
    }
  }
  if ((static_cast<unsigned>(value) &
       ~static_cast<unsigned>(DirtyFlags::All)) != 0)
    result += result.empty() ? "Unknown" : "|Unknown";
  return result;
}
constexpr std::string_view toString(RepeatLayout value) {
  switch (value) {
  case RepeatLayout::Stack:
    return "Stack";
  case RepeatLayout::Grid:
    return "Grid";
  case RepeatLayout::Flow:
    return "Flow";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::RepeatLayout>
    : std::formatter<std::string_view> {
  auto format(playground::ui::RepeatLayout value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(LayoutPhase value) {
  switch (value) {
  case LayoutPhase::Measure:
    return "Measure";
  case LayoutPhase::Arrange:
    return "Arrange";
  case LayoutPhase::Prepare:
    return "Prepare";
  case LayoutPhase::Paint:
    return "Paint";
  case LayoutPhase::Input:
    return "Input";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::LayoutPhase>
    : std::formatter<std::string_view> {
  auto format(playground::ui::LayoutPhase value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(LayoutIssue value) {
  switch (value) {
  case LayoutIssue::UnboundedFill:
    return "UnboundedFill";
  case LayoutIssue::IndefinitePercent:
    return "IndefinitePercent";
  case LayoutIssue::ConstraintViolation:
    return "ConstraintViolation";
  case LayoutIssue::AspectConflict:
    return "AspectConflict";
  case LayoutIssue::FontDoesNotFit:
    return "FontDoesNotFit";
  case LayoutIssue::NonInvertibleTransform:
    return "NonInvertibleTransform";
  case LayoutIssue::IndefiniteAnchor:
    return "IndefiniteAnchor";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::LayoutIssue>
    : std::formatter<std::string_view> {
  auto format(playground::ui::LayoutIssue value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {

constexpr std::string_view toString(Visibility value) {
  switch (value) {
  case Visibility::Visible:
    return "Visible";
  case Visibility::Hidden:
    return "Hidden";
  case Visibility::Collapsed:
    return "Collapsed";
  }
  return "Unknown";
}

constexpr std::string_view toString(HitTestPolicy value) {
  switch (value) {
  case HitTestPolicy::None:
    return "None";
  case HitTestPolicy::ChildrenOnly:
    return "ChildrenOnly";
  case HitTestPolicy::Self:
    return "Self";
  case HitTestPolicy::SelfAndChildren:
    return "SelfAndChildren";
  }
  return "Unknown";
}

constexpr std::string_view toString(SemanticRole value) {
  switch (value) {
  case SemanticRole::None:
    return "None";
  case SemanticRole::Group:
    return "Group";
  case SemanticRole::Button:
    return "Button";
  case SemanticRole::Text:
    return "Text";
  case SemanticRole::Image:
    return "Image";
  case SemanticRole::ScrollArea:
    return "ScrollArea";
  }
  return "Unknown";
}

constexpr std::string_view toString(EventType value) {
  switch (value) {
  case EventType::PointerMove:
    return "PointerMove";
  case EventType::PointerDown:
    return "PointerDown";
  case EventType::PointerUp:
    return "PointerUp";
  case EventType::PointerCancel:
    return "PointerCancel";
  case EventType::PointerEnter:
    return "PointerEnter";
  case EventType::PointerLeave:
    return "PointerLeave";
  case EventType::Wheel:
    return "Wheel";
  case EventType::KeyDown:
    return "KeyDown";
  case EventType::KeyUp:
    return "KeyUp";
  case EventType::FocusLost:
    return "FocusLost";
  case EventType::FocusGained:
    return "FocusGained";
  }
  return "Unknown";
}

constexpr std::string_view toString(EventPhase value) {
  switch (value) {
  case EventPhase::Capture:
    return "Capture";
  case EventPhase::Target:
    return "Target";
  case EventPhase::Bubble:
    return "Bubble";
  }
  return "Unknown";
}

constexpr std::string_view toString(Key value) {
  switch (value) {
  case Key::Unknown:
    return "Unknown";
  case Key::Space:
    return "Space";
  case Key::Enter:
    return "Enter";
  case Key::Tab:
    return "Tab";
  case Key::Escape:
    return "Escape";
  case Key::Left:
    return "Left";
  case Key::Right:
    return "Right";
  case Key::Up:
    return "Up";
  case Key::Down:
    return "Down";
  }
  return "Unknown";
}

constexpr std::string_view toString(TextWrap value) {
  switch (value) {
  case TextWrap::None:
    return "None";
  case TextWrap::AvailableInlineSize:
    return "AvailableInlineSize";
  }
  return "Unknown";
}

constexpr std::string_view toString(FontFit value) {
  switch (value) {
  case FontFit::None:
    return "None";
  case FontFit::ShrinkToFit:
    return "ShrinkToFit";
  }
  return "Unknown";
}

constexpr std::string_view toString(TextMethod value) {
  switch (value) {
  case TextMethod::Blended:
    return "Blended";
  case TextMethod::Solid:
    return "Solid";
  case TextMethod::Shaded:
    return "Shaded";
  case TextMethod::LCD:
    return "LCD";
  }
  return "Unknown";
}

constexpr std::string_view toString(ContentFit value) {
  switch (value) {
  case ContentFit::None:
    return "None";
  case ContentFit::Stretch:
    return "Stretch";
  case ContentFit::Contain:
    return "Contain";
  case ContentFit::Cover:
    return "Cover";
  case ContentFit::Shrink:
    return "Shrink";
  }
  return "Unknown";
}

constexpr std::string_view toString(ItemExtentMode value) {
  switch (value) {
  case ItemExtentMode::Fixed:
    return "Fixed";
  case ItemExtentMode::Estimated:
    return "Estimated";
  }
  return "Unknown";
}

constexpr std::string_view toString(ScrollAxes value) {
  switch (value) {
  case ScrollAxes::Horizontal:
    return "Horizontal";
  case ScrollAxes::Vertical:
    return "Vertical";
  case ScrollAxes::Both:
    return "Both";
  }
  return "Unknown";
}

constexpr std::string_view toString(ScrollbarPolicy value) {
  switch (value) {
  case ScrollbarPolicy::Never:
    return "Never";
  case ScrollbarPolicy::Auto:
    return "Auto";
  case ScrollbarPolicy::Always:
    return "Always";
  }
  return "Unknown";
}

constexpr std::string_view toString(LayerCachePolicy value) {
  switch (value) {
  case LayerCachePolicy::None:
    return "None";
  case LayerCachePolicy::WhenUnchanged:
    return "WhenUnchanged";
  }
  return "Unknown";
}

} // namespace playground::ui

template <>
struct std::formatter<playground::ui::Visibility>
    : std::formatter<std::string_view> {
  auto format(playground::ui::Visibility value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::HitTestPolicy>
    : std::formatter<std::string_view> {
  auto format(playground::ui::HitTestPolicy value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::SemanticRole>
    : std::formatter<std::string_view> {
  auto format(playground::ui::SemanticRole value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::EventType>
    : std::formatter<std::string_view> {
  auto format(playground::ui::EventType value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::EventPhase>
    : std::formatter<std::string_view> {
  auto format(playground::ui::EventPhase value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::Key> : std::formatter<std::string_view> {
  auto format(playground::ui::Key value, std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::TextWrap>
    : std::formatter<std::string_view> {
  auto format(playground::ui::TextWrap value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::FontFit>
    : std::formatter<std::string_view> {
  auto format(playground::ui::FontFit value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::TextMethod>
    : std::formatter<std::string_view> {
  auto format(playground::ui::TextMethod value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::ContentFit>
    : std::formatter<std::string_view> {
  auto format(playground::ui::ContentFit value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::ItemExtentMode>
    : std::formatter<std::string_view> {
  auto format(playground::ui::ItemExtentMode value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::ScrollAxes>
    : std::formatter<std::string_view> {
  auto format(playground::ui::ScrollAxes value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::ScrollbarPolicy>
    : std::formatter<std::string_view> {
  auto format(playground::ui::ScrollbarPolicy value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

template <>
struct std::formatter<playground::ui::DirtyFlags>
    : std::formatter<std::string_view> {
  auto format(playground::ui::DirtyFlags value,
              std::format_context &ctx) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), ctx);
  }
};

template <>
struct std::formatter<playground::ui::LayerCachePolicy>
    : std::formatter<std::string_view> {
  auto format(playground::ui::LayerCachePolicy value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(TextTruncation value) {
  switch (value) {
  case TextTruncation::None:
    return "None";
  case TextTruncation::EllipsisStart:
    return "EllipsisStart";
  case TextTruncation::EllipsisMiddle:
    return "EllipsisMiddle";
  case TextTruncation::EllipsisEnd:
    return "EllipsisEnd";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::TextTruncation>
    : std::formatter<std::string_view> {
  auto format(playground::ui::TextTruncation value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(WritingMode value) {
  switch (value) {
  case WritingMode::HorizontalTb:
    return "HorizontalTb";
  case WritingMode::VerticalRl:
    return "VerticalRl";
  case WritingMode::VerticalLr:
    return "VerticalLr";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::WritingMode>
    : std::formatter<std::string_view> {
  auto format(playground::ui::WritingMode value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(TextOrientation value) {
  switch (value) {
  case TextOrientation::Mixed:
    return "Mixed";
  case TextOrientation::Upright:
    return "Upright";
  case TextOrientation::Sideways:
    return "Sideways";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::TextOrientation>
    : std::formatter<std::string_view> {
  auto format(playground::ui::TextOrientation value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};

namespace playground::ui {
constexpr std::string_view toString(CollectionOperation value) {
  switch (value) {
  case CollectionOperation::Insert:
    return "Insert";
  case CollectionOperation::Erase:
    return "Erase";
  case CollectionOperation::Move:
    return "Move";
  case CollectionOperation::Update:
    return "Update";
  }
  return "Unknown";
}
} // namespace playground::ui
template <>
struct std::formatter<playground::ui::CollectionOperation>
    : std::formatter<std::string_view> {
  auto format(playground::ui::CollectionOperation value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::ui::toString(value), context);
  }
};
