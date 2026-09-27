#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "layout/LayoutAlgorithms.hpp"

namespace {
using namespace playground::layout;
using playground::Patch;

void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

void near(float actual, float expected, std::string_view message) {
  require(std::isfinite(actual) && std::abs(actual - expected) <= 0.001f,
          message);
}

template <typename Exception = std::invalid_argument, typename Function>
void rejects(Function &&function, std::string_view message) {
  try {
    function();
  } catch (const Exception &) {
    return;
  }
  throw std::runtime_error(std::string{message});
}

void patches() {
  require(playground::Patch<int>{}.isKeep(), "Default patch must Keep");
  require(playground::Patch<int>::keep().appliedTo(8, 4) == 8,
          "Keep preserves current");
  require(playground::Patch<int>::set(0).appliedTo(8, 4) == 0,
          "Set accepts zero");
  require(!playground::Patch<bool>::set(false).appliedTo(true, true),
          "Set accepts false");
  require(playground::Patch<int>::reset().appliedTo(8, 4) == 4,
          "Reset uses baseline");
  rejects([] { (void)playground::Patch<int>::reset().appliedTo(8); },
          "Required value without baseline rejects Reset");
  using OptionalPatch = playground::Patch<std::optional<float>>;
  require(!OptionalPatch::set(std::nullopt).appliedTo(3.0f, 7.0f),
          "Set nullopt clears an optional");
  require(OptionalPatch::reset().appliedTo(3.0f, 7.0f) == 7.0f,
          "Reset optional restores nonempty baseline");
  require(playground::Patch<int>::set(9).value() &&
              *playground::Patch<int>::set(9).value() == 9,
          "Set exposes its value");
  require(playground::Patch<int>::keep().value() == nullptr,
          "Keep has no replacement value");

  BoxProps props;
  props.maxWidth = 100.0f;
  BoxPatch patch;
  patch.maxWidth = playground::Patch<std::optional<float>>::set(std::nullopt);
  require(!patched(props, patch).maxWidth, "Box patch clears maximum");
  patch.maxWidth = playground::Patch<std::optional<float>>::set(10.0f);
  patch.minWidth = playground::Patch<float>::set(20);
  rejects([&] { (void)patched(props, patch); },
          "Box patch validates combined fields");
  require(props.maxWidth == 100 && props.minWidth == 0,
          "Rejected patch does not mutate source");
}

void constraintsAndSizing() {
  const auto limits = AxisConstraints::bounded(10, 100);
  near(limits.clamp(5), 10, "Clamp minimum");
  near(limits.clamp(120), 100, "Clamp maximum");
  require(limits.deflated(200) == AxisConstraints::tight(0),
          "Deflation clamps at zero");
  require(!AxisConstraints::unbounded().deflated(10).maximum,
          "Deflation preserves unbounded maximum");
  require(SizeConstraints::tight({100, 50}).deflated(Insets::all(5)) ==
              SizeConstraints::tight({90, 40}),
          "Insets deflate both axes");
  rejects([] { AxisConstraints{20.0f, 10.0f}.validate(); },
          "Reject reversed limits");
  rejects([] { (void)SizeRule::fixed(-1); }, "Reject negative fixed extent");
  rejects(
      [] { (void)SizeRule::percent(std::numeric_limits<float>::infinity()); },
      "Reject authored infinity");
  rejects(
      [] {
        AxisConstraints{std::numeric_limits<float>::quiet_NaN(), {}}.validate();
      },
      "Reject NaN minimum");
  near(resolveSizeRule(SizeRule::fixed(200), 30, limits), 100,
       "Hard maximum beats fixed request");
  near(resolveSizeRule(SizeRule::percent(0.5f), 30, limits, 80.0f), 40,
       "Percentage uses explicit basis");
  near(resolveSizeRule(SizeRule::percent(0.5f), 30, limits), 30,
       "Indefinite percentage falls back to content");
  near(resolveSizeRule(SizeRule::fill(), 30, limits), 100,
       "Finite Fill uses maximum");
  near(resolveSizeRule(SizeRule::fill(), 30, {}), 30,
       "Unbounded Fill uses content");
  rejects<std::overflow_error>(
      [] {
        (void)resolveSizeRule(SizeRule::percent(2), 0, {},
                              std::numeric_limits<float>::max());
      },
      "Reject percentage extent overflow");
  MeasureResult{{100, 20}, 10.0f, 18.0f}.validate();
  rejects([] { MeasureResult{{100, 20}, 25.0f, {}}.validate(); },
          "Reject outside baseline");
  rejects([] { MeasureResult{{100, 20}, 18.0f, 10.0f}.validate(); },
          "Reject reversed baselines");
}

void flexAndDistribution() {
  std::array grow{FlexItem{10, {0, 50.0f}, 1}, FlexItem{10, {}, 2}};
  const auto grown = allocateStack(grow, 320);
  near(grown.sizes[0], 50, "Growth freezes at maximum");
  near(grown.sizes[1], 270, "Growth redistributes frozen remainder");
  near(grown.remaining, 0, "Growth conserves available extent");

  std::array shrink{FlexItem{100, {90, {}}, 0, 1},
                    FlexItem{100, {20, {}}, 0, 1}};
  const auto shrunk = allocateStack(shrink, 120);
  near(shrunk.sizes[0], 90, "Shrink freezes at minimum");
  near(shrunk.sizes[1], 30, "Shrink redistributes frozen deficit");
  const auto overflow = allocateStack(shrink, 50);
  near(overflow.sizes[0], 90, "Overflow retains first minimum");
  near(overflow.sizes[1], 20, "Overflow retains second minimum");
  near(overflow.remaining, -60, "Unresolved deficit is negative remaining");

  std::array weighted{FlexItem{100, {}, 0, 1}, FlexItem{200, {}, 0, 1}};
  const auto weightedResult = allocateStack(weighted, 150);
  near(weightedResult.sizes[0], 50, "Shrink weight includes initial basis");
  near(weightedResult.sizes[1], 100, "Larger basis takes larger deficit");
  std::array locked{FlexItem{80, {}, 1, 1, true}, FlexItem{20, {}, 1, 1}};
  const auto lockedResult = allocateStack(locked, 150, 10);
  near(lockedResult.sizes[0], 80, "Fixed child cannot grow");
  near(lockedResult.sizes[1], 60, "Gap deducted before allocation");
  std::array zero{FlexItem{0, {10, {}}, 0, 1}};
  near(allocateStack(zero, 0).remaining, -10,
       "Zero basis has no shrinkable weight");
  require(allocateStack({}, 100, 10).sizes.empty(),
          "Empty stack has no phantom gap");
  near(allocateStack({}, 100, 10).remaining, 100,
       "Empty stack retains all space");

  auto distribution =
      distributionOffsets(Distribution::SpaceBetween, 40, 3, 10);
  near(distribution.between, 30, "SpaceBetween adds free space to gap");
  near(distributionOffsets(Distribution::SpaceBetween, 40, 1).leading, 0,
       "Single-item SpaceBetween means Start");
  distribution = distributionOffsets(Distribution::SpaceAround, 40, 2, 10);
  near(distribution.leading, 10, "SpaceAround half leading space");
  near(distribution.between, 30, "SpaceAround inter-item space");
  distribution = distributionOffsets(Distribution::SpaceEvenly, 30, 2, 10);
  near(distribution.leading, 10, "SpaceEvenly leading space");
  near(distribution.between, 20, "SpaceEvenly preserves minimum gap");
  near(distributionOffsets(Distribution::Center, -20, 2).leading, 0,
       "Overflow does not manufacture negative distribution");
  near(distributionOffsets(Distribution::SpaceAround, 20, 0).leading, 0,
       "Empty distribution avoids division by zero");
}

void placementAndFlow() {
  near(alignmentOffset(Align::Start, 100, 20, true), 80,
       "RTL Start is right edge");
  near(alignmentOffset(Align::End, 100, 20, true), 0, "RTL End is left edge");
  near(alignmentOffset(Align::Center, 100, 120), 0,
       "Oversized LTR child uses safe Start");
  near(alignmentOffset(Align::Center, 100, 120, true), -20,
       "Oversized RTL child overflows toward logical End");
  const auto aligned = alignBounds({{10, 20}, {100, 50}}, {20, 10},
                                   Alignment::center(), Insets::all(5));
  require(aligned == Rect{{50, 40}, {20, 10}},
          "Alignment respects origin and margins");
  const auto stretched = alignBounds({{0, 0}, {100, 50}}, {20, 10},
                                     Alignment::stretch(), Insets::all(5));
  require(stretched == Rect{{5, 5}, {90, 40}},
          "Stretch uses interior margin box");
  std::array extents{60.0f, 30.0f, 120.0f, 20.0f};
  const auto lines = breakFlowLines(extents, 100.0f, 10);
  require(lines.size() == 3 && lines[0].begin == 0 && lines[0].end == 2 &&
              lines[1].begin == 2 && lines[1].end == 3 && lines[2].end == 4,
          "Flow wraps and keeps oversized item on one nonempty line");
  near(lines[0].mainExtent, 100, "Flow exact-fit line includes gap");
  require(breakFlowLines(extents, std::nullopt, 10).size() == 1,
          "Unbounded Flow has one line");
  require(breakFlowLines({}, 0.0f).empty(), "Empty Flow has no lines");
  StackProps{0, Distribution::Start, CrossAlignment::FirstBaseline}.validate();
  rejects(
      [] {
        StackProps{0, Distribution::Start, CrossAlignment::FirstBaseline}
            .validate(Axis::Vertical);
      },
      "Vertical stack rejects baseline alignment");
}

void tracksAndAnchors() {
  const auto fixed = TrackSize::fixed(20);
  require(fixed.kind() == TrackKind::Fixed &&
              fixed.limits() == AxisConstraints::tight(20),
          "Fixed track locks its limits");
  const auto fraction = TrackSize::fraction(2, {10, 100.0f});
  require(fraction.kind() == TrackKind::Fraction && fraction.value() == 2 &&
              fraction.limits().maximum == 100,
          "Fraction retains weight and bounds");
  rejects([] { (void)TrackSize::fraction(0); }, "Reject zero fraction weight");
  rejects([] { GridPlacement{{}, {}, 0, 1, {}, {}}.validate(); },
          "Reject zero grid span");
  rejects(
      [] {
        GridPlacement{std::numeric_limits<std::size_t>::max(), {}, 1, 1, {}, {}}
            .validate();
      },
      "Reject overflowing grid span");
  GridProps grid;
  grid.columns.clear();
  rejects([&] { grid.validate(); }, "Nonempty grid requires tracks");
  grid.validate(false);

  auto anchored = resolveAnchor(AnchorPosition::center(5), 100, 20);
  near(anchored.position, 45, "Anchor combines parent, self, and offset");
  near(resolveAnchor(AnchorPosition::start(5), 100, 20, false, true).position,
       75, "RTL anchor reflects logical offset");
  anchored = resolveAnchor(StretchBetween{10, 20}, 100, 0);
  near(anchored.position, 10, "Stretch anchor start");
  near(anchored.extent, 70, "Stretch anchor extent");
  near(resolveAnchor(StretchBetween{10, 20}, 100, 0, false, true).position, 20,
       "RTL stretch anchor maps insets");
  near(resolveAnchor(StretchBetween{80, 80}, 100, 0).extent, 0,
       "Exhausted stretch clamps extent at zero");
  rejects([] { (void)resolveAnchor(StretchBetween{}, 100, 20, true); },
          "Fixed child conflicts with StretchBetween");
  rejects([] { AnchorPosition{1.1f, 0, 0}.validate(); },
          "Reject anchor fractions outside unit range");
}
} // namespace

int main() {
  try {
    patches();
    constraintsAndSizing();
    flexAndDistribution();
    placementAndFlow();
    tracksAndAnchors();
    std::cout << "Layout value tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Layout value test failed: " << error.what() << '\n';
    return 1;
  }
}
