#include <limits>
#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Box.hpp>

using namespace playground;

int main() {
  return test::run([] {
    for (auto axes : {ui::ScrollAxes::Horizontal, ui::ScrollAxes::Vertical,
                      ui::ScrollAxes::Both})
      for (auto bars : {ui::ScrollbarPolicy::Never, ui::ScrollbarPolicy::Auto,
                        ui::ScrollbarPolicy::Always}) {
        auto scroll = std::make_unique<ui::ScrollView>(
            nullptr, ui::ScrollProps{.axes = axes, .scrollbar = bars});
        auto *s = scroll.get();
        s->setChild(std::make_unique<ui::Box>(
            layout::BoxProps{.width = layout::SizeRule::fixed(300),
                             .height = layout::SizeRule::fixed(200)}));
        ui::UIRoot root;
        root.setContent(std::move(scroll));
        root.flushLayout({100, 80});
        s->setOffset({10000, 10000});
        root.flushLayout({100, 80});
        test::require(
            s->offset().x == (axes == ui::ScrollAxes::Vertical
                                  ? 0
                                  : 300 - s->viewportExtent().width) &&
                s->offset().y == (axes == ui::ScrollAxes::Horizontal
                                      ? 0
                                      : 200 - s->viewportExtent().height),
            "scroll clamps each enabled axis");
        s->setOffset({-10, -10});
        test::require(s->offset() == math::Vec2f{},
                      "negative offset clamps to zero");
        test::rejects(
            [&] { s->setOffset({std::numeric_limits<float>::quiet_NaN(), 0}); },
            "NaN offset rejected");
        const auto props = s->props();
        test::rejects(
            [&] {
              s->applyPatch({.wheelStep = playground::Patch<float>::set(-1)});
            },
            "negative wheel step rejected");
        test::require(s->props() == props,
                      "failed scroll patch leaves props unchanged");
        auto old = s->takeChild();
        root.flushLayout({100, 80});
        test::require(old && s->contentExtent() == math::Size2{} &&
                          s->offset() == math::Vec2f{},
                      "removing scroll content clears derived range");
      }
    // Nested local arrangement must visit both queued nodes even when a
    // cached intermediate container lets the outer arrangement skip them.
    auto inner = std::make_unique<ui::ScrollView>(
        std::make_unique<ui::Box>(
            layout::BoxProps{.height = layout::SizeRule::fixed(600)}),
        ui::ScrollProps{},
        layout::BoxProps{.width = layout::SizeRule::fixed(180),
                         .height = layout::SizeRule::fixed(150)});
    auto *inside = inner.get();
    inside->child()->setHitTestPolicy(ui::HitTestPolicy::SelfAndChildren);
    auto box = std::make_unique<ui::Box>(
        layout::BoxProps{.height = layout::SizeRule::fixed(500)});
    box->setChild(std::move(inner));
    auto outer = std::make_unique<ui::ScrollView>(std::move(box));
    auto *outside = outer.get();
    ui::UIRoot root;
    root.setContent(std::move(outer));
    root.flushLayout({200, 200});
    root.update(0);
    const auto measured = root.stats().measured;
    outside->setOffset({0, 5});
    inside->setOffset({0, 20});
    root.flushLayout({200, 200});
    test::require(
        outside->child()->bounds().y() == -5 &&
            inside->child()->bounds().y() == -20 &&
            root.stats().measured == measured,
        "nested arrangement queues preserve geometry without measurement");
    const auto offset = inside->offset();
    const auto viewport = inside->viewportExtent();
    root.preferredSize({1000, 1000});
    test::require(inside->offset() == offset &&
                      inside->viewportExtent() == viewport,
                  "preferred offers do not mutate nested live scroll state");
    root.flushLayout({200, 200});
    const auto point = inside->worldTransform().mapPoint({10, 10});
    test::require(root.hitTest(point) &&
                      root.hitTest(point)->target.id() == inside->child()->id(),
                  "scroll placement updates descendant hit coordinates");
    ui::UIEvent wheel{
        .type = ui::EventType::Wheel, .position = point, .delta = {0, 100}};
    root.dispatch(wheel);
    root.flushLayout({200, 200});
    test::require(
        inside->offset().y == inside->contentExtent().height -
                                  inside->viewportExtent().height &&
            outside->offset().y > 5,
        "nested wheel consumes its range then forwards residual movement");
    auto smaller = inside->child()->boxProps();
    smaller.height = layout::SizeRule::fixed(40);
    inside->child()->setBoxProps(smaller);
    root.flushLayout({220, 240});
    test::require(
        inside->offset().y == 0 && inside->contentExtent().height == 40,
        "content shrink and resize invalidate cached scroll geometry");
  });
}
