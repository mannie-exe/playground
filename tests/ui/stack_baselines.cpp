#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Stack.hpp>
#include <ui/controls/Choice.hpp>

using namespace playground;

namespace {
std::unique_ptr<ui::Node> leaf(float height, std::optional<float> first,
                               std::optional<float> last) {
  return std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
      .measure = [=](ui::MeasureContext &, const layout::SizeConstraints &) {
        return layout::MeasureResult{{20, height}, first, last};
      }});
}
} // namespace

int main() {
  return test::run([] {
    ui::MeasureContext context;
    // Mixed authored padding must not invert the exported baseline range.
    ui::HStack buttons;
    buttons.append(std::make_unique<ui::Button>(
        leaf(20, 15.f, 15.f), ui::ButtonProps{},
        layout::BoxProps{.padding = math::Insets::all(10)}));
    buttons.append(std::make_unique<ui::Button>(leaf(20, 15.f, 15.f)));
    const auto measured = buttons.measure(context, {});
    test::require(measured.firstBaseline == 15 && measured.lastBaseline == 25,
                  "mixed padding exports ordered positioned baselines");

    constexpr std::array alignments{layout::CrossAlignment::Start,
                                    layout::CrossAlignment::Center,
                                    layout::CrossAlignment::End,
                                    layout::CrossAlignment::Stretch,
                                    layout::CrossAlignment::FirstBaseline,
                                    layout::CrossAlignment::LastBaseline};
    for (auto direction : {layout::LayoutDirection::LeftToRight,
                           layout::LayoutDirection::RightToLeft}) {
      for (auto first : alignments) {
        for (auto second : alignments) {
          for (bool reverse : {false, true}) {
            auto row = std::make_unique<ui::HStack>();
            auto *stack = row.get();
            auto tall = leaf(40, 30.f, 35.f);
            auto shortLeaf = leaf(20, 5.f, 10.f);
            auto *a = tall.get();
            auto *b = shortLeaf.get();
            const layout::StackPlacement tallPlacement{
                .margin = {.top = 3, .bottom = 5},
                .crossAlignmentOverride = first};
            const layout::StackPlacement shortPlacement{
                .margin = {.top = 7, .bottom = 2},
                .crossAlignmentOverride = second};
            if (reverse) {
              row->append(std::move(shortLeaf), shortPlacement);
              row->append(std::move(tall), tallPlacement);
            } else {
              row->append(std::move(tall), tallPlacement);
              row->append(std::move(shortLeaf), shortPlacement);
            }
            context.direction = direction;
            const auto result = stack->measure(context, {});
            result.validate();
            ui::UIRoot root;
            root.setContent(std::move(row));
            root.flushLayout(result.size, direction);
            test::require(
                result.firstBaseline ==
                        std::min(a->bounds().y() + 30, b->bounds().y() + 5) &&
                    result.lastBaseline ==
                        std::max(a->bounds().y() + 35, b->bounds().y() + 10),
                "exported envelope matches positioned children for every "
                "alignment, order and direction");
          }
        }
      }
    }

    ui::HStack partial;
    partial.append(leaf(40, 30.f, {}));
    partial.append(leaf(20, {}, 10.f));
    auto collapsed = leaf(100, 1.f, 99.f);
    collapsed->setVisibility(ui::Visibility::Collapsed);
    partial.append(std::move(collapsed));
    const auto sparse = partial.measure(context, {});
    test::require(sparse.firstBaseline == 10 && sparse.lastBaseline == 30,
                  "partial baseline data forms a range; collapsed children "
                  "do not contribute");
    ui::HStack empty;
    const auto none = empty.measure(context, {});
    test::require(!none.firstBaseline && !none.lastBaseline,
                  "empty rows export no baselines");
    empty.append(leaf(20, {}, {}));
    const auto baselineFree = empty.measure(context, {});
    test::require(!baselineFree.firstBaseline && !baselineFree.lastBaseline,
                  "ordinary rows do not invent baselines");
    ui::VStack vertical;
    vertical.append(leaf(20, 5.f, 15.f));
    const auto column = vertical.measure(context, {});
    test::require(!column.firstBaseline && !column.lastBaseline,
                  "vertical stacks retain their no-baseline contract");
    ui::HStack fallback{
        {.childrenAlignment = layout::CrossAlignment::FirstBaseline}};
    fallback.append(leaf(20, {}, {}));
    const auto synthesized = fallback.measure(context, {});
    test::require(synthesized.firstBaseline == 20 &&
                      synthesized.lastBaseline == 20,
                  "explicit baseline alignment uses bottom-edge fallback");
    test::rejects(
        [&] { leaf(20, 15.f, 5.f)->measure(context, {}); },
        "invalid child baseline order is rejected, not repaired by parent");
  });
}
