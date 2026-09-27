#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

int main() {
  return test::run([] {
    auto rectangle = std::make_unique<ui::Rectangle>(
        ui::RectangleProps{},
        layout::BoxProps{.width = layout::SizeRule::fixed(400),
                         .height = layout::SizeRule::fixed(300)});
    auto scroll = std::make_unique<ui::ScrollView>(
        std::move(rectangle),
        ui::ScrollProps{.axes = ui::ScrollAxes::Both,
                        .sizing = ui::ScrollSizing::Content});
    auto *view = scroll.get();
    ui::UIRoot root;
    root.setContent(std::move(scroll));
    test::require(
        root.preferredSize({1000, 800}, {1, 1}) == math::Size2{400, 300},
        "natural content size does not inflate to available work area");
    test::require(root.preferredSize({250, 200}, {1, 1}) ==
                      math::Size2{250, 200},
                  "preferred size clamps only to actual available size");
    root.flushLayout({250, 200});
    test::require(view->contentExtent() == math::Size2{400, 300},
                  "overflow preserves full content extent");
    view->setOffset({999, 999});
    test::require(view->offset() == math::Vec2f{158, 108},
                  "overflow scrolls to content end");
    root.flushLayout({400, 300});
    test::require(view->offset() == math::Vec2f{},
                  "fitting window needs no scrolling");
    ui::MeasureContext context;
    test::require(view->measure(context, {}).size == math::Size2{400, 300},
                  "content sizing supports natural unbounded measurement");
    view->applyPatch({.sizing = Patch<ui::ScrollSizing>::reset()});
    bool rejected{};
    try {
      (void)view->measure(context, {});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    test::require(rejected,
                  "legacy Fill still requires finite viewport constraints");
  });
}
