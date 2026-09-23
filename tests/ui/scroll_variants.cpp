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
            s->offset().x == (axes == ui::ScrollAxes::Vertical ? 0 : 200) &&
                s->offset().y == (axes == ui::ScrollAxes::Horizontal ? 0 : 120),
            "scroll clamps each enabled axis");
        s->setOffset({-10, -10});
        test::require(s->offset() == math::Vec2f{},
                      "negative offset clamps to zero");
        test::rejects(
            [&] { s->setOffset({std::numeric_limits<float>::quiet_NaN(), 0}); },
            "NaN offset rejected");
        const auto props = s->props();
        test::rejects(
            [&] { s->applyPatch({.wheelStep = ui::Patch<float>::set(-1)}); },
            "negative wheel step rejected");
        test::require(s->props() == props,
                      "failed scroll patch leaves props unchanged");
        auto old = s->takeChild();
        root.flushLayout({100, 80});
        test::require(old && s->contentExtent() == math::Size2{} &&
                          s->offset() == math::Vec2f{},
                      "removing scroll content clears derived range");
      }
  });
}
