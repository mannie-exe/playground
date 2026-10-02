#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include "math/Geometry2D.hpp"
#include "support/Patch.hpp"

namespace playground::layout {

using math::Gap2;
using math::Insets;
using math::Point2;
using math::Rect;
using math::Size2;

enum class Axis { Horizontal, Vertical };
enum class LayoutDirection { LeftToRight, RightToLeft };
enum class Align { Start, Center, End, Stretch };
enum class CrossAlignment {
  Start,
  Center,
  End,
  Stretch,
  FirstBaseline,
  LastBaseline
};
enum class Distribution {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly
};
enum class OverflowPolicy { Visible, Clip };
enum class SizeKind { Content, Fixed, Percent, Fill };
enum class TrackKind { Fixed, Content, Fraction };
enum class LengthKind { Units, Percent };

class Length {
  LengthKind _kind{LengthKind::Units};
  float _value{};

  Length(LengthKind kind, float value) : _kind{kind}, _value{value} {
    if (!std::isfinite(value) || value < 0)
      throw std::invalid_argument("Length must be finite and nonnegative");
  }

public:
  Length() = default;

  static Length units(float value) { return {LengthKind::Units, value}; }

  static Length percent(float value) { return {LengthKind::Percent, value}; }

  LengthKind kind() const noexcept { return _kind; }

  float value() const noexcept { return _value; }

  std::optional<float> resolve(std::optional<float> basis) const {
    if (_kind == LengthKind::Units)
      return _value;
    if (!basis)
      return {};
    if (!std::isfinite(*basis) || *basis < 0)
      throw std::invalid_argument("Invalid length basis");
    const double value = static_cast<double>(*basis) * _value;
    if (value > std::numeric_limits<float>::max())
      throw std::overflow_error("Length exceeds float range");
    return static_cast<float>(value);
  }

  bool operator==(const Length &) const = default;
};

namespace detail {
inline void nonnegative(float value, const char *message) {
  if (!std::isfinite(value) || value < 0)
    throw std::invalid_argument(message);
}

inline void finite(float value, const char *message) {
  if (!std::isfinite(value))
    throw std::invalid_argument(message);
}

inline void insets(const Insets &value) {
  nonnegative(value.top, "Inset must be finite and nonnegative");
  nonnegative(value.right, "Inset must be finite and nonnegative");
  nonnegative(value.bottom, "Inset must be finite and nonnegative");
  nonnegative(value.left, "Inset must be finite and nonnegative");
}

inline float checked(double value) {
  if (!std::isfinite(value) ||
      value > static_cast<double>(std::numeric_limits<float>::max()))
    throw std::overflow_error("Layout extent exceeds float range");
  return static_cast<float>(value);
}
} // namespace detail

struct Alignment {
  Align horizontal{Align::Start};
  Align vertical{Align::Start};

  static constexpr Alignment center() { return {Align::Center, Align::Center}; }

  static constexpr Alignment stretch() {
    return {Align::Stretch, Align::Stretch};
  }

  bool operator==(const Alignment &) const = default;
};

struct AxisConstraints {
  float minimum{};
  std::optional<float> maximum;

  void validate() const {
    detail::nonnegative(minimum, "Minimum must be finite and nonnegative");
    if (maximum) {
      detail::nonnegative(*maximum, "Maximum must be finite and nonnegative");
      if (*maximum < minimum)
        throw std::invalid_argument("Maximum is below minimum");
    }
  }

  static AxisConstraints tight(float value) {
    AxisConstraints result{value, value};
    result.validate();
    return result;
  }

  static AxisConstraints bounded(float minimum, float maximum) {
    AxisConstraints result{minimum, maximum};
    result.validate();
    return result;
  }

  static AxisConstraints unbounded(float minimum = 0) {
    AxisConstraints result{minimum, std::nullopt};
    result.validate();
    return result;
  }

  float clamp(float value) const {
    validate();
    detail::nonnegative(value,
                        "Measured extent must be finite and nonnegative");
    return maximum ? std::clamp(value, minimum, *maximum)
                   : std::max(value, minimum);
  }

  AxisConstraints deflated(float amount) const {
    validate();
    detail::nonnegative(amount, "Deflation must be finite and nonnegative");
    return {std::max(0.0f, minimum - amount),
            maximum ? std::optional{std::max(0.0f, *maximum - amount)}
                    : std::nullopt};
  }

  bool operator==(const AxisConstraints &) const = default;
};

struct SizeConstraints {
  AxisConstraints width;
  AxisConstraints height;

  void validate() const {
    width.validate();
    height.validate();
  }

  static SizeConstraints tight(Size2 size) {
    return {AxisConstraints::tight(size.width),
            AxisConstraints::tight(size.height)};
  }

  Size2 clamp(Size2 value) const {
    return {width.clamp(value.width), height.clamp(value.height)};
  }

  SizeConstraints deflated(Insets value) const {
    detail::insets(value);
    return {width.deflated(
                detail::checked(static_cast<double>(value.left) + value.right)),
            height.deflated(detail::checked(static_cast<double>(value.top) +
                                            value.bottom))};
  }

  bool operator==(const SizeConstraints &) const = default;
};

class SizeRule {
  SizeKind _kind{SizeKind::Content};
  float _value{};

  constexpr SizeRule(SizeKind kind, float value) : _kind(kind), _value(value) {}

public:
  constexpr SizeRule() = default;

  static constexpr SizeRule content() { return {}; }

  static SizeRule fixed(float units) {
    detail::nonnegative(units, "Fixed size must be finite and nonnegative");
    return {SizeKind::Fixed, units};
  }

  static SizeRule percent(float fraction) {
    detail::nonnegative(fraction, "Percentage must be finite and nonnegative");
    return {SizeKind::Percent, fraction};
  }

  static constexpr SizeRule fill() { return {SizeKind::Fill, 0}; }

  constexpr SizeKind kind() const noexcept { return _kind; }

  constexpr float value() const noexcept { return _value; }

  bool operator==(const SizeRule &) const = default;
};

struct BoxProps {
  SizeRule width;
  SizeRule height;
  float minWidth{};
  float minHeight{};
  std::optional<float> maxWidth;
  std::optional<float> maxHeight;
  Insets padding;
  Insets borderWidths;
  std::optional<float> aspectRatio;

  void validate() const {
    AxisConstraints{minWidth, maxWidth}.validate();
    AxisConstraints{minHeight, maxHeight}.validate();
    detail::insets(padding);
    detail::insets(borderWidths);
    if (aspectRatio && (!std::isfinite(*aspectRatio) || *aspectRatio <= 0))
      throw std::invalid_argument("Aspect ratio must be finite and positive");
  }

  bool operator==(const BoxProps &) const = default;
};

struct BoxPatch {
  Patch<SizeRule> width;
  Patch<SizeRule> height;
  Patch<float> minWidth;
  Patch<float> minHeight;
  Patch<std::optional<float>> maxWidth;
  Patch<std::optional<float>> maxHeight;
  Patch<Insets> padding;
  Patch<Insets> borderWidths;
  Patch<std::optional<float>> aspectRatio;
};

inline void validate(const BoxProps &props) { props.validate(); }

inline BoxProps patched(const BoxProps &props, const BoxPatch &patch) {
  const BoxProps baseline;
  BoxProps result{
      patch.width.appliedTo(props.width, baseline.width),
      patch.height.appliedTo(props.height, baseline.height),
      patch.minWidth.appliedTo(props.minWidth, baseline.minWidth),
      patch.minHeight.appliedTo(props.minHeight, baseline.minHeight),
      patch.maxWidth.appliedTo(props.maxWidth, baseline.maxWidth),
      patch.maxHeight.appliedTo(props.maxHeight, baseline.maxHeight),
      patch.padding.appliedTo(props.padding, baseline.padding),
      patch.borderWidths.appliedTo(props.borderWidths, baseline.borderWidths),
      patch.aspectRatio.appliedTo(props.aspectRatio, baseline.aspectRatio)};
  result.validate();
  return result;
}

struct MeasureResult {
  Size2 size;
  std::optional<float> firstBaseline;
  std::optional<float> lastBaseline;

  void validate() const {
    detail::nonnegative(size.width,
                        "Measured width must be finite and nonnegative");
    detail::nonnegative(size.height,
                        "Measured height must be finite and nonnegative");
    for (const auto baseline : {firstBaseline, lastBaseline}) {
      if (baseline && (!std::isfinite(*baseline) || *baseline < 0 ||
                       *baseline > size.height))
        throw std::invalid_argument(
            "Baseline is outside the measured border box");
    }
    if (firstBaseline && lastBaseline && *firstBaseline > *lastBaseline)
      throw std::invalid_argument("First baseline is after last baseline");
  }

  bool operator==(const MeasureResult &) const = default;
};

struct LayoutResult {
  Rect borderBounds;
  Rect contentBounds;
  Rect overflowBounds;
  bool operator==(const LayoutResult &) const = default;
};

struct BoxPlacement {
  Insets margin;
  std::optional<Alignment> alignmentOverride;

  void validate() const { detail::insets(margin); }

  bool operator==(const BoxPlacement &) const = default;
};

using LayerPlacement = BoxPlacement;

struct StackPlacement {
  Insets margin;
  float grow{};
  float shrink{1};
  std::optional<CrossAlignment> crossAlignmentOverride;

  void validate() const {
    detail::insets(margin);
    detail::nonnegative(grow, "Grow weight must be finite and nonnegative");
    detail::nonnegative(shrink, "Shrink weight must be finite and nonnegative");
  }

  bool operator==(const StackPlacement &) const = default;
};

struct StackProps {
  std::optional<float> gap;
  Distribution distribution{Distribution::Start};
  CrossAlignment childrenAlignment{CrossAlignment::Start};

  void validate(Axis axis = Axis::Horizontal) const {
    if (gap)
      detail::nonnegative(*gap, "Stack gap must be finite and nonnegative");
    if (axis == Axis::Vertical &&
        (childrenAlignment == CrossAlignment::FirstBaseline ||
         childrenAlignment == CrossAlignment::LastBaseline))
      throw std::invalid_argument(
          "Vertical stacks do not support baseline alignment");
  }

  bool operator==(const StackProps &) const = default;
};

class TrackSize {
  TrackKind _kind{TrackKind::Content};
  float _value{};
  AxisConstraints _limits;

  TrackSize(TrackKind kind, float value, AxisConstraints limits)
      : _kind(kind), _value(value), _limits(limits) {}

public:
  TrackSize() = default;

  static TrackSize fixed(float units) {
    return {TrackKind::Fixed, units, AxisConstraints::tight(units)};
  }

  static TrackSize content(AxisConstraints limits = {}) {
    limits.validate();
    return {TrackKind::Content, 0, limits};
  }

  static TrackSize fraction(float weight = 1, AxisConstraints limits = {}) {
    if (!std::isfinite(weight) || weight <= 0)
      throw std::invalid_argument(
          "Fraction track weight must be finite and positive");
    limits.validate();
    return {TrackKind::Fraction, weight, limits};
  }

  TrackKind kind() const noexcept { return _kind; }

  float value() const noexcept { return _value; }

  const AxisConstraints &limits() const noexcept { return _limits; }

  bool operator==(const TrackSize &) const = default;
};

struct GridPlacement {
  std::optional<std::size_t> row;
  std::optional<std::size_t> column;
  std::size_t rowSpan{1};
  std::size_t columnSpan{1};
  Insets margin;
  std::optional<Alignment> alignmentOverride;

  void validate() const {
    detail::insets(margin);
    if (!rowSpan || !columnSpan)
      throw std::invalid_argument("Grid spans must be positive");
    if ((row && *row > std::numeric_limits<std::size_t>::max() - rowSpan) ||
        (column &&
         *column > std::numeric_limits<std::size_t>::max() - columnSpan))
      throw std::invalid_argument("Grid span exceeds index range");
  }

  bool operator==(const GridPlacement &) const = default;
};

struct GridProps {
  std::vector<TrackSize> columns{TrackSize::content()};
  std::vector<TrackSize> rows{TrackSize::content()};
  Gap2 gap;
  Axis autoPlacementAxis{Axis::Horizontal};
  Alignment childrenAlignment{Alignment::stretch()};
  bool allowOverlap{};
  TrackSize implicitTrack;

  void validate(bool hasChildren = true) const {
    detail::nonnegative(gap.horizontal,
                        "Grid gap must be finite and nonnegative");
    detail::nonnegative(gap.vertical,
                        "Grid gap must be finite and nonnegative");
    if (hasChildren && (rows.empty() || columns.empty()))
      throw std::invalid_argument(
          "A nonempty grid needs row and column tracks");
  }

  bool operator==(const GridProps &) const = default;
};

struct FlowProps {
  Axis mainAxis{Axis::Horizontal};
  std::optional<float> itemGap, lineGap;
  Distribution distribution{Distribution::Start};
  CrossAlignment childrenAlignment{CrossAlignment::Start};

  void validate() const {
    StackProps{itemGap, distribution, childrenAlignment}.validate(mainAxis);
    detail::nonnegative(lineGap.value_or(0),
                        "Flow line gap must be finite and nonnegative");
  }

  bool operator==(const FlowProps &) const = default;
};

struct AnchorPosition {
  float parentFraction{};
  float selfFraction{};
  float offset{};

  void validate() const {
    if (!std::isfinite(parentFraction) || parentFraction < 0 ||
        parentFraction > 1 || !std::isfinite(selfFraction) ||
        selfFraction < 0 || selfFraction > 1)
      throw std::invalid_argument(
          "Anchor fractions must be between zero and one");
    detail::finite(offset, "Anchor offset must be finite");
  }

  static AnchorPosition start(float offset = 0) { return {0, 0, offset}; }

  static AnchorPosition center(float offset = 0) {
    return {0.5f, 0.5f, offset};
  }

  static AnchorPosition end(float offset = 0) { return {1, 1, offset}; }

  bool operator==(const AnchorPosition &) const = default;
};

struct StretchBetween {
  float startInset{};
  float endInset{};

  void validate() const {
    detail::nonnegative(startInset,
                        "Anchor inset must be finite and nonnegative");
    detail::nonnegative(endInset,
                        "Anchor inset must be finite and nonnegative");
  }

  bool operator==(const StretchBetween &) const = default;
};

using AnchorAxis = std::variant<AnchorPosition, StretchBetween>;

struct AnchorPlacement {
  AnchorAxis horizontal{AnchorPosition{}};
  AnchorAxis vertical{AnchorPosition{}};
  Insets margin;

  void validate() const {
    std::visit([](const auto &axis) { axis.validate(); }, horizontal);
    std::visit([](const auto &axis) { axis.validate(); }, vertical);
    detail::insets(margin);
  }

  bool operator==(const AnchorPlacement &) const = default;
};

} // namespace playground::layout
