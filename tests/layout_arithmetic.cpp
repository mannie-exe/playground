#include <array>
#include <cmath>
#include <limits>
#include <numeric>

#include <layout/LayoutAlgorithms.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    // Deterministic parameter sweep, not timing or a probabilistic assertion.
    for (int seed = 0; seed < 200; ++seed) {
      std::array<layout::FlexItem, 5> items;
      for (int i = 0; i < 5; ++i) {
        const float minimum = static_cast<float>((seed + i * 3) % 17);
        items[i] = {.basis = static_cast<float>((seed * 7 + i * 11) % 90),
                    .limits = {minimum, minimum + 40},
                    .grow = static_cast<float>((seed + i) % 4),
                    .shrink = static_cast<float>((seed + i * 2) % 3),
                    .fixed = (seed + i) % 7 == 0};
      }
      for (float available : {0.f, 20.f, 100.f, 250.f, 1000.f}) {
        const float gap = static_cast<float>(seed % 5);
        const auto actual = layout::allocateStack(items, available, gap);
        const auto repeat = layout::allocateStack(items, available, gap);
        test::require(actual.sizes == repeat.sizes &&
                          actual.remaining == repeat.remaining,
                      "flex allocation is deterministic");
        double sum = gap * 4;
        for (std::size_t i = 0; i < items.size(); ++i) {
          const auto value = actual.sizes[i];
          test::require(std::isfinite(value) &&
                            value >= items[i].limits.minimum &&
                            value <= *items[i].limits.maximum,
                        "flex respects finite limits");
          if (items[i].fixed)
            test::require(value == items[i].limits.clamp(items[i].basis),
                          "fixed item unchanged");
          sum += value;
        }
        test::require(
            std::abs(sum + actual.remaining - available) < 0.001,
            "sizes + gaps + signed remainder conserve available space");
      }
    }
    for (auto mode :
         {layout::Distribution::Start, layout::Distribution::Center,
          layout::Distribution::End, layout::Distribution::SpaceBetween,
          layout::Distribution::SpaceAround,
          layout::Distribution::SpaceEvenly}) {
      for (std::size_t count : {0u, 1u, 2u, 7u}) {
        for (float free : {-40.f, 0.f, 80.f}) {
          const auto offset = layout::distributionOffsets(mode, free, count, 3);
          test::require(
              offset.leading >= 0 && offset.between >= 3,
              "distribution never manufactures negative gaps or leading space");
          if (count > 1 && free > 0 &&
              mode == layout::Distribution::SpaceBetween)
            test::require(std::abs((offset.between - 3) * (count - 1) - free) <
                              0.001,
                          "SpaceBetween consumes exactly the free space");
        }
      }
    }
    for (auto align : {layout::Align::Start, layout::Align::Center,
                       layout::Align::End, layout::Align::Stretch}) {
      for (float available : {0.f, 30.f, 100.f})
        for (float extent : {0.f, 20.f, 150.f})
          test::require(
              math::almostEqual(
                  layout::alignmentOffset(align, available, extent) +
                      layout::alignmentOffset(align, available, extent, true),
                  available - extent),
              "RTL is the reflection of LTR even during overflow");
    }
    for (int seed = 0; seed < 100; ++seed) {
      std::array<float, 8> extents;
      for (int i = 0; i < 8; ++i)
        extents[i] = static_cast<float>((seed * 13 + i * 7) % 120);
      const auto lines = layout::breakFlowLines(extents, 100.f, 5);
      std::size_t next{};
      for (const auto &line : lines) {
        test::require(line.begin == next && line.end > line.begin,
                      "flow partitions each item once");
        const float sum = std::accumulate(extents.begin() + line.begin,
                                          extents.begin() + line.end, 0.f) +
                          5 * (line.end - line.begin - 1);
        test::require(sum == line.mainExtent,
                      "flow extent includes exactly the internal gaps");
        test::require(sum <= 100 || line.end - line.begin == 1,
                      "only a single item can overflow a line");
        next = line.end;
      }
      test::require(next == extents.size(), "flow loses no items");
    }
    test::rejects([] { layout::allocateStack({}, -1); },
                  "negative available extent rejected");
    test::rejects(
        [] {
          layout::allocateStack(std::array{layout::FlexItem{.grow = -1}}, 100);
        },
        "negative weight rejected");
    test::rejects([] { layout::breakFlowLines(std::array{-1.f}, 100.f); },
                  "negative item rejected");
    test::rejects(
        [] {
          layout::distributionOffsets(layout::Distribution::Start,
                                      std::numeric_limits<float>::quiet_NaN(),
                                      2);
        },
        "NaN distribution rejected");
  });
}
