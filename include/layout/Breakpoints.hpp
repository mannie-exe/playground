#pragma once

#include <string>
#include <utility>

#include <layout/LayoutPrimitives.hpp>

namespace playground::layout {

// Half-open logical-size ranges. Unknown/unbounded dimensions match only
// an unrestricted axis, never a guessed display size or an infinity sentinel.
struct SizeRange {
  math::Size2 minimum{};
  std::optional<float> maximumWidth, maximumHeight;

  void validate() const {
    AxisConstraints{minimum.width, maximumWidth}.validate();
    AxisConstraints{minimum.height, maximumHeight}.validate();
    if ((maximumWidth && *maximumWidth == minimum.width) ||
        (maximumHeight && *maximumHeight == minimum.height))
      throw std::invalid_argument("Breakpoint range must not be empty");
  }
  bool contains(const SizeConstraints &space) const {
    validate();
    space.validate();
    const auto matches = [](float min, std::optional<float> max,
                            std::optional<float> available) {
      return available ? *available >= min && (!max || *available < *max)
                       : min == 0 && !max;
    };
    return matches(minimum.width, maximumWidth, space.width.maximum) &&
           matches(minimum.height, maximumHeight, space.height.maximum);
  }
  bool overlaps(const SizeRange &other) const {
    validate();
    other.validate();
    const auto intersects = [](float a, std::optional<float> aEnd, float b,
                               std::optional<float> bEnd) {
      return (!bEnd || a < *bEnd) && (!aEnd || b < *aEnd);
    };
    return intersects(minimum.width, maximumWidth, other.minimum.width,
                      other.maximumWidth) &&
           intersects(minimum.height, maximumHeight, other.minimum.height,
                      other.maximumHeight);
  }
  bool operator==(const SizeRange &) const = default;
};

template <class Mode> struct Breakpoint {
  SizeRange availableSpace;
  Mode mode;
  std::string name;
  bool operator==(const Breakpoint &) const = default;
};

template <class Mode> struct BreakpointProps {
  std::vector<Breakpoint<Mode>> rules;
  Mode fallback;
  bool operator==(const BreakpointProps &) const = default;
};

template <class Mode> struct BreakpointPatch {
  ui::Patch<std::vector<Breakpoint<Mode>>> rules;
  ui::Patch<Mode> fallback;
};

template <class Mode> class BreakpointSet {
  BreakpointProps<Mode> _props;

public:
  explicit BreakpointSet(BreakpointProps<Mode> props)
      : _props{std::move(props)} {
    validate(_props);
  }
  static void validate(const BreakpointProps<Mode> &props) {
    for (std::size_t i = 0; i < props.rules.size(); ++i) {
      props.rules[i].availableSpace.validate();
      for (std::size_t j = 0; j < i; ++j)
        if (props.rules[i].availableSpace.overlaps(
                props.rules[j].availableSpace))
          throw std::invalid_argument("Breakpoint ranges overlap");
    }
  }
  const BreakpointProps<Mode> &props() const noexcept { return _props; }
  void setProps(BreakpointProps<Mode> props) {
    validate(props);
    _props = std::move(props);
  }
  // Reset needs an explicit policy: there is no universal default Mode.
  void applyPatch(const BreakpointPatch<Mode> &patch,
                  const BreakpointProps<Mode> &defaults) {
    setProps({patch.rules.appliedTo(_props.rules, defaults.rules),
              patch.fallback.appliedTo(_props.fallback, defaults.fallback)});
  }
  Mode select(const SizeConstraints &space) const {
    space.validate();
    for (const auto &rule : _props.rules)
      if (rule.availableSpace.contains(space))
        return rule.mode;
    return _props.fallback;
  }
  Mode select(math::Size2 availableSpace) const {
    return select(SizeConstraints::tight(availableSpace));
  }
};

} // namespace playground::layout
