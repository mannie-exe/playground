#include <iostream>
#include <stdexcept>

#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/AnchorLayout.hpp>
#include <ui/containers/Flow.hpp>
#include <ui/containers/Grid.hpp>
#include <ui/containers/ZStack.hpp>
#include <ui/controls/Button.hpp>

namespace ui = playground::ui;
namespace math = playground::math;
namespace layout = playground::layout;
static void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
class Leaf : public ui::Node {
  math::Size2 _natural;

protected:
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &offered) override {
    const float width = offered.width.maximum.value_or(_natural.width);
    return {{_natural.width, width < _natural.width ? 40.0f : _natural.height},
            10.0f,
            10.0f};
  }

public:
  explicit Leaf(math::Size2 size = {20, 20}, layout::BoxProps props = {})
      : Node{props}, _natural{size} {}
};
int main() {
  try {
    ui::UIRoot root;
    auto stack = std::make_unique<ui::HStack>(layout::StackProps{.gap = 10});
    auto *a = &stack->append(std::make_unique<Leaf>(), {.grow = 1});
    auto *b = &stack->append(std::make_unique<Leaf>(), {.grow = 2});
    root.setContent(std::move(stack));
    root.flushLayout({150, 60});
    check(math::almostEqual(a->bounds().w(), 53.3333f) &&
              math::almostEqual(b->bounds().w(), 86.6667f),
          "stack weighted grow");
    check(math::almostEqual(b->bounds().x(), 63.3333f), "stack gap");
    root.flushLayout(math::Size2{150, 60},
                     layout::LayoutDirection::RightToLeft);
    check(a->bounds().x() > b->bounds().x(), "RTL stack placement");
    auto box = std::make_unique<ui::Box>(
        layout::BoxProps{.padding = math::Insets::all(10)},
        ui::BoxContentProps{layout::Alignment::center()});
    auto *leaf = &box->setChild(std::make_unique<Leaf>());
    root.setContent(std::move(box));
    root.flushLayout({100, 80});
    check(leaf->bounds().position == math::Point2{40, 30},
          "nested box padding alignment");
    auto grid = std::make_unique<ui::Grid>(
        layout::GridProps{.columns = {layout::TrackSize::fraction(),
                                      layout::TrackSize::fraction()},
                          .gap = {4, 4}});
    auto *g0 = &grid->append(std::make_unique<Leaf>());
    auto *g1 = &grid->append(std::make_unique<Leaf>());
    auto *g2 = &grid->append(std::make_unique<Leaf>(), {.columnSpan = 2});
    root.setContent(std::move(grid));
    root.flushLayout({104, 100});
    check(g0->bounds().w() == 50 && g1->bounds().x() == 54,
          "fraction grid columns");
    check(g2->bounds().w() == 104 && g2->bounds().y() > 0,
          "grid implicit row and span");
    auto spanning = std::make_unique<ui::Grid>(
        layout::GridProps{.columns = {layout::TrackSize::content()}});
    auto *wide = &spanning->append(std::make_unique<Leaf>(), {.columnSpan = 3});
    root.setContent(std::move(spanning));
    root.flushLayout({100, 100});
    check(wide->bounds().y() == 0,
          "oversized automatic span begins in first row");
    auto overlap = std::make_unique<ui::Grid>(layout::GridProps{
        .columns = {layout::TrackSize::content()}, .allowOverlap = true});
    overlap->append(std::make_unique<Leaf>(), {.row = 0, .column = 0});
    overlap->append(std::make_unique<Leaf>(), {.row = 0, .column = 0});
    auto policy = overlap->props();
    policy.allowOverlap = false;
    bool overlapRejected{};
    try {
      overlap->setProps(policy);
    } catch (const std::invalid_argument &) {
      overlapRejected = true;
    }
    check(overlapRejected && overlap->props().allowOverlap,
          "overlap policy validates before commit");
    auto flow = std::make_unique<ui::Flow>(
        layout::FlowProps{.itemGap = 5, .lineGap = 3});
    auto *f0 = &flow->append(std::make_unique<Leaf>(math::Size2{30, 20}));
    auto *f1 = &flow->append(std::make_unique<Leaf>(math::Size2{30, 20}));
    root.setContent(std::move(flow));
    root.flushLayout({50, 100});
    check(f1->bounds().y() == f0->bounds().h() + 3, "flow wraps lines");
    auto anchor = std::make_unique<ui::AnchorLayout>();
    auto *anchored =
        &anchor->append(std::make_unique<Leaf>(),
                        {.horizontal = layout::AnchorPosition::end(-4),
                         .vertical = layout::AnchorPosition::center()});
    root.setContent(std::move(anchor));
    root.flushLayout({100, 80});
    check(anchored->bounds().position == math::Point2{76, 30},
          "anchor positioning");
    auto button = std::make_unique<ui::Button>();
    auto *control = button.get();
    int clicks{};
    auto connection = control->onActivate([&] { ++clicks; });
    root.setContent(std::move(button));
    root.flushLayout({100, 80});
    ui::UIEvent down{.type = ui::EventType::PointerDown,
                     .position = {10, 10},
                     .pointer = 7,
                     .button = 1};
    root.dispatch(down);
    ui::UIEvent wrong{.type = ui::EventType::PointerUp,
                      .position = {10, 10},
                      .pointer = 7,
                      .button = 2};
    root.dispatch(wrong);
    check(control->isPressed() && clicks == 0,
          "unrelated release cannot activate");
    ui::UIEvent up{.type = ui::EventType::PointerUp,
                   .position = {200, 200},
                   .pointer = 7,
                   .button = 1};
    root.dispatch(up);
    check(!control->isPressed() && clicks == 0,
          "outside release cancels activation");
    down.handled = false;
    root.dispatch(down);
    control->setEnabled(false);
    up.position = {10, 10};
    up.handled = false;
    root.dispatch(up);
    check(clicks == 0 && !control->isPressed(), "disable mid-press");
    control->setEnabled(true);
    root.requestFocus(control->id());
    ui::UIEvent keyDown{.type = ui::EventType::KeyDown,
                        .logicalKey = ui::Key::Space};
    root.dispatch(keyDown);
    ui::UIEvent keyUp{.type = ui::EventType::KeyUp,
                      .logicalKey = ui::Key::Space};
    root.dispatch(keyUp);
    check(clicks == 1, "keyboard activation");
    auto scroll = std::make_unique<ui::ScrollView>(std::make_unique<ui::Button>(
        nullptr, ui::ButtonProps{},
        layout::BoxProps{.width = layout::SizeRule::fixed(100),
                         .height = layout::SizeRule::fixed(400)}));
    auto *scrollPtr = scroll.get();
    root.setContent(std::move(scroll));
    root.flushLayout({100, 100});
    const auto hit = root.hitTest({96, 10});
    check(hit && hit->target.get() == scrollPtr,
          "scrollbar overlays child input");
    ui::UIEvent dragDown{.type = ui::EventType::PointerDown,
                         .position = {96, 10},
                         .pointer = 4,
                         .button = 1};
    root.dispatch(dragDown);
    ui::UIEvent dragMove{
        .type = ui::EventType::PointerMove, .position = {96, 60}, .pointer = 4};
    root.dispatch(dragMove);
    check(scrollPtr->offset().y > 0, "scrollbar drag updates offset");
    std::cout << "Container/control tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
