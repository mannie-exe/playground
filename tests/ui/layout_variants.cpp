#include <limits>
#include <memory>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/AdaptiveStack.hpp>
#include <ui/containers/Box.hpp>
#include <ui/containers/Stack.hpp>

using namespace playground;

static std::unique_ptr<ui::Node> leaf(float height, float first, float last) {
  return std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
      .measure = [=](ui::MeasureContext &, const layout::SizeConstraints &) {
        return layout::MeasureResult{{20, height}, first, last};
      }});
}

int main() {
  return test::run([] {
    for (auto alignment :
         {layout::CrossAlignment::Start, layout::CrossAlignment::Center,
          layout::CrossAlignment::End, layout::CrossAlignment::Stretch,
          layout::CrossAlignment::FirstBaseline,
          layout::CrossAlignment::LastBaseline}) {
      auto stack = std::make_unique<ui::HStack>(
          layout::StackProps{.childrenAlignment = alignment});
      auto &a = stack->append(leaf(20, 10, 15));
      auto &b = stack->append(leaf(40, 30, 32));
      ui::UIRoot root;
      root.setContent(std::move(stack));
      root.flushLayout({100, 60});
      if (alignment == layout::CrossAlignment::FirstBaseline)
        test::require(
            a.bounds().y() + 10 == b.bounds().y() + 30,
            "baseline alignment accounts for different child ascents");
      if (alignment == layout::CrossAlignment::LastBaseline)
        test::require(
            a.bounds().y() + 15 == b.bounds().y() + 32,
            "last baseline is distinct from first baseline alignment");
      if (alignment == layout::CrossAlignment::Start)
        test::require(a.bounds().y() == 0 && b.bounds().y() == 0,
                      "cross Start");
      if (alignment == layout::CrossAlignment::Center)
        test::require(a.bounds().y() == 20 && b.bounds().y() == 10,
                      "cross Center");
      if (alignment == layout::CrossAlignment::End)
        test::require(a.bounds().y() == 40 && b.bounds().y() == 20,
                      "cross End");
      if (alignment == layout::CrossAlignment::Stretch)
        test::require(a.bounds().h() == 60 && b.bounds().h() == 60,
                      "cross Stretch");
    }
    test::require(layout::Length::units(5).resolve({}) == 5,
                  "absolute length needs no basis");
    test::require(!layout::Length::percent(0.5f).resolve({}),
                  "indefinite percentage remains unresolved");
    test::require(layout::Length::percent(0.5f).resolve(80.f) == 40,
                  "percentage uses basis");
    test::rejects<std::overflow_error>(
        [] {
          layout::Length::percent(2).resolve(std::numeric_limits<float>::max());
        },
        "length multiplication overflow rejected");
    ui::Box box;
    const auto original = box.Node::settings();
    test::rejects(
        [&] {
          box.Node::applySettingsPatch(
              {.paint = {.opacity = playground::Patch<float>::set(2)}});
        },
        "out-of-range opacity rejected");
    test::rejects(
        [&] {
          box.applyBoxPatch({.padding = playground::Patch<math::Insets>::set(
                                 math::Insets::all(-1))});
        },
        "negative insets rejected");
    test::rejects(
        [&] {
          box.applyVisualPatch(
              {.clipRect = playground::Patch<std::optional<math::Rect>>::set(
                   math::rect(0, 0, -1, 10))});
        },
        "invalid clip rejected");
    test::require(box.Node::settings() == original,
                  "invalid common patches do not partially commit");
    test::rejects(
        [] {
          ui::AdaptiveStack invalid{ui::AdaptiveStackProps{
              .breakpoints = {.rules = {{.availableSpace = {.minimum = {10, 0}},
                                         .mode = layout::Axis::Horizontal},
                                        {.availableSpace = {.minimum = {20, 0}},
                                         .mode = layout::Axis::Vertical}}}}};
        },
        "overlapping adaptive conditions rejected");
    ui::UIRoot root;
    test::rejects(
        [&] {
          root.flushLayout(ui::LayoutEnvironment{.viewport = {100, 100},
                                                 .pixelScale = {0, 1}});
        },
        "invalid pixel density rejected at environment boundary");
  });
}
