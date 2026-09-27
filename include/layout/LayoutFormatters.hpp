#pragma once

#include <format>
#include <string_view>

#include <layout/Constraints.hpp>
#include <layout/LayoutPrimitives.hpp>

namespace playground::layout {

constexpr std::string_view toString(Axis value) {
  switch (value) {
  case Axis::Horizontal:
    return "Horizontal";
  case Axis::Vertical:
    return "Vertical";
  }
  return "Unknown";
}

constexpr std::string_view toString(LayoutDirection value) {
  switch (value) {
  case LayoutDirection::LeftToRight:
    return "LeftToRight";
  case LayoutDirection::RightToLeft:
    return "RightToLeft";
  }
  return "Unknown";
}

constexpr std::string_view toString(Align value) {
  switch (value) {
  case Align::Start:
    return "Start";
  case Align::Center:
    return "Center";
  case Align::End:
    return "End";
  case Align::Stretch:
    return "Stretch";
  }
  return "Unknown";
}

constexpr std::string_view toString(CrossAlignment value) {
  switch (value) {
  case CrossAlignment::Start:
    return "Start";
  case CrossAlignment::Center:
    return "Center";
  case CrossAlignment::End:
    return "End";
  case CrossAlignment::Stretch:
    return "Stretch";
  case CrossAlignment::FirstBaseline:
    return "FirstBaseline";
  case CrossAlignment::LastBaseline:
    return "LastBaseline";
  }
  return "Unknown";
}

constexpr std::string_view toString(Distribution value) {
  switch (value) {
  case Distribution::Start:
    return "Start";
  case Distribution::Center:
    return "Center";
  case Distribution::End:
    return "End";
  case Distribution::SpaceBetween:
    return "SpaceBetween";
  case Distribution::SpaceAround:
    return "SpaceAround";
  case Distribution::SpaceEvenly:
    return "SpaceEvenly";
  }
  return "Unknown";
}

constexpr std::string_view toString(OverflowPolicy value) {
  switch (value) {
  case OverflowPolicy::Visible:
    return "Visible";
  case OverflowPolicy::Clip:
    return "Clip";
  }
  return "Unknown";
}

constexpr std::string_view toString(SizeKind value) {
  switch (value) {
  case SizeKind::Content:
    return "Content";
  case SizeKind::Fixed:
    return "Fixed";
  case SizeKind::Percent:
    return "Percent";
  case SizeKind::Fill:
    return "Fill";
  }
  return "Unknown";
}

constexpr std::string_view toString(TrackKind value) {
  switch (value) {
  case TrackKind::Fixed:
    return "Fixed";
  case TrackKind::Content:
    return "Content";
  case TrackKind::Fraction:
    return "Fraction";
  }
  return "Unknown";
}

constexpr std::string_view toString(LengthKind value) {
  switch (value) {
  case LengthKind::Units:
    return "Units";
  case LengthKind::Percent:
    return "Percent";
  }
  return "Unknown";
}

} // namespace playground::layout

template <>
struct std::formatter<playground::layout::Axis>
    : std::formatter<std::string_view> {
  auto format(playground::layout::Axis value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::LayoutDirection>
    : std::formatter<std::string_view> {
  auto format(playground::layout::LayoutDirection value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::Align>
    : std::formatter<std::string_view> {
  auto format(playground::layout::Align value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::CrossAlignment>
    : std::formatter<std::string_view> {
  auto format(playground::layout::CrossAlignment value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::Distribution>
    : std::formatter<std::string_view> {
  auto format(playground::layout::Distribution value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::OverflowPolicy>
    : std::formatter<std::string_view> {
  auto format(playground::layout::OverflowPolicy value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::SizeKind>
    : std::formatter<std::string_view> {
  auto format(playground::layout::SizeKind value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::TrackKind>
    : std::formatter<std::string_view> {
  auto format(playground::layout::TrackKind value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

template <>
struct std::formatter<playground::layout::LengthKind>
    : std::formatter<std::string_view> {
  auto format(playground::layout::LengthKind value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

namespace playground::layout {
constexpr std::string_view toString(AnchorAttribute value) {
  switch (value) {
  case AnchorAttribute::Start:
    return "Start";
  case AnchorAttribute::End:
    return "End";
  case AnchorAttribute::Top:
    return "Top";
  case AnchorAttribute::Bottom:
    return "Bottom";
  case AnchorAttribute::CenterX:
    return "CenterX";
  case AnchorAttribute::CenterY:
    return "CenterY";
  case AnchorAttribute::Width:
    return "Width";
  case AnchorAttribute::Height:
    return "Height";
  case AnchorAttribute::Baseline:
    return "Baseline";
  }
  return "Unknown";
}
} // namespace playground::layout

template <>
struct std::formatter<playground::layout::AnchorAttribute>
    : std::formatter<std::string_view> {
  auto format(playground::layout::AnchorAttribute value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

namespace playground::layout {
constexpr std::string_view toString(ConstraintRelation value) {
  switch (value) {
  case ConstraintRelation::Equal:
    return "Equal";
  case ConstraintRelation::LessEqual:
    return "LessEqual";
  case ConstraintRelation::GreaterEqual:
    return "GreaterEqual";
  }
  return "Unknown";
}
} // namespace playground::layout

template <>
struct std::formatter<playground::layout::ConstraintRelation>
    : std::formatter<std::string_view> {
  auto format(playground::layout::ConstraintRelation value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};

namespace playground::layout {
constexpr std::string_view toString(ConstraintStrength value) {
  switch (value) {
  case ConstraintStrength::Required:
    return "Required";
  case ConstraintStrength::Strong:
    return "Strong";
  case ConstraintStrength::Medium:
    return "Medium";
  case ConstraintStrength::Weak:
    return "Weak";
  }
  return "Unknown";
}
} // namespace playground::layout

template <>
struct std::formatter<playground::layout::ConstraintStrength>
    : std::formatter<std::string_view> {
  auto format(playground::layout::ConstraintStrength value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::layout::toString(value), context);
  }
};
