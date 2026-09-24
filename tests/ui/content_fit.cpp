#include <cmath>

#include <support/Test.hpp>
#include <ui/content/ContentTypes.hpp>

using namespace playground;

int main() {
  return test::run([] {
    for (auto fit : {ui::ContentFit::None, ui::ContentFit::Stretch,
                     ui::ContentFit::Contain, ui::ContentFit::Cover,
                     ui::ContentFit::Shrink})
      for (auto x :
           {layout::Align::Start, layout::Align::Center, layout::Align::End})
        for (auto y :
             {layout::Align::Start, layout::Align::Center, layout::Align::End})
          for (auto direction : {layout::LayoutDirection::LeftToRight,
                                 layout::LayoutDirection::RightToLeft})
            for (auto target :
                 {math::rect(10, 20, 80, 80), math::rect(10, 20, 20, 10)}) {
              const auto source = math::rect(4, 8, 80, 40);
              const math::Size2 natural{40, 20};
              const ui::ContentStyle style{.fit = fit, .alignment = {x, y}};
              ui::content_detail::validate(style);
              const auto r = ui::content_detail::resolve(
                  source, natural, target, style, direction);
              test::require(math::isFinite(r.source) &&
                                math::isFinite(r.destination),
                            "finite resolved geometry");
              test::require(r.source.left() >= source.left() &&
                                r.source.right() <= source.right() &&
                                r.source.top() >= source.top() &&
                                r.source.bottom() <= source.bottom(),
                            "crop stays in source");
              if (fit == ui::ContentFit::Stretch ||
                  fit == ui::ContentFit::Cover)
                test::require(r.destination == target,
                              "stretch and cover fill destination");
              if (fit == ui::ContentFit::Contain ||
                  fit == ui::ContentFit::Shrink) {
                test::require(r.destination.w() <= target.w() &&
                                  r.destination.h() <= target.h(),
                              "contain fits inside box");
                test::require(math::almostEqual(
                                  r.destination.w() / r.destination.h(), 2.f),
                              "contain preserves aspect");
              }
              if (fit == ui::ContentFit::None)
                test::require(r.destination.size == natural,
                              "none preserves logical size");
              if (fit == ui::ContentFit::Shrink)
                test::require(r.destination.w() <= natural.width,
                              "shrink never enlarges");
              if (fit != ui::ContentFit::Cover)
                test::require(r.source == source, "only cover crops");
            }
    const auto empty = ui::content_detail::resolve(
        math::rect(0, 0, 0, 10), {0, 10}, math::rect(0, 0, 50, 50), {}, {});
    test::require(!empty.destination.hasArea(), "zero source skips drawing");
    test::rejects(
        [] {
          ui::content_detail::validate(
              {.fit = static_cast<ui::ContentFit>(99)});
        },
        "unknown fit rejected");
    test::rejects(
        [] {
          ui::content_detail::validate(
              {.alignment = layout::Alignment::stretch()});
        },
        "stretch alignment rejected");
    test::rejects(
        [] {
          ui::content_detail::validate(
              {.paint = {.sampling = static_cast<rendering::Sampling>(99)}});
        },
        "unknown sampling rejected");
  });
}
