#include <limits>

#include <layout/Breakpoints.hpp>
#include <support/Test.hpp>

using namespace playground;
enum class Mode { Compact, Wide, Unknown };

int main() {
  return test::run([] {
    const layout::BreakpointProps<Mode> defaults{
        .rules = {{.availableSpace = {.maximumWidth = 600.0f},
                   .mode = Mode::Compact},
                  {.availableSpace = {.minimum = {600, 0}},
                   .mode = Mode::Wide}},
        .fallback = Mode::Unknown};
    layout::BreakpointSet<Mode> modes{defaults};
    test::require(modes.select(math::Size2{599, 50}) == Mode::Compact &&
                      modes.select(math::Size2{600, 50}) == Mode::Wide,
                  "adjacent half-open ranges have no ambiguity at boundary");
    test::require(modes.select(layout::SizeConstraints{}) == Mode::Unknown,
                  "unknown width is not a guessed wide screen");
    layout::SizeConstraints widthOnly;
    widthOnly.width = layout::AxisConstraints::bounded(0, 700);
    test::require(modes.select(widthOnly) == Mode::Wide,
                  "unrestricted height accepts unknown height");
    layout::SizeRange box{
        .minimum = {10, 20}, .maximumWidth = 30.0f, .maximumHeight = 40.0f};
    test::require(box.contains(layout::SizeConstraints::tight({10, 20})) &&
                      !box.contains(layout::SizeConstraints::tight({30, 20})) &&
                      !box.contains(layout::SizeConstraints::tight({10, 40})),
                  "both axes have inclusive minimum and exclusive maximum");
    for (auto invalid :
         {layout::SizeRange{.minimum = {-1, 0}},
          layout::SizeRange{.maximumWidth = 0.0f},
          layout::SizeRange{.minimum = {20, 0}, .maximumWidth = 10.0f},
          layout::SizeRange{.maximumHeight =
                                std::numeric_limits<float>::infinity()},
          layout::SizeRange{
              .minimum = {std::numeric_limits<float>::quiet_NaN(), 0}}})
      test::rejects([&] { invalid.validate(); }, "invalid range rejected");
    auto overlap = defaults;
    overlap.rules[1].availableSpace.minimum.width = 599;
    test::rejects([&] { modes.setProps(overlap); }, "overlap rejected");
    test::require(modes.props() == defaults,
                  "invalid replacement preserves live rules");
    modes.applyPatch(
        {.rules = ui::Patch<std::vector<layout::Breakpoint<Mode>>>::set({}),
         .fallback = ui::Patch<Mode>::set(Mode::Compact)},
        defaults);
    test::require(modes.select(math::Size2{900, 50}) == Mode::Compact,
                  "empty rule set selects authored fallback");
    modes.applyPatch({}, defaults);
    test::require(modes.props().rules.empty(),
                  "Keep does not restore defaults");
    modes.applyPatch(
        {.rules = ui::Patch<std::vector<layout::Breakpoint<Mode>>>::reset(),
         .fallback = ui::Patch<Mode>::reset()},
        defaults);
    test::require(modes.props() == defaults,
                  "Reset uses explicit caller policy");
  });
}
