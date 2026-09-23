#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Box.hpp>
#include <ui/containers/ConstraintLayout.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;
using A = layout::AnchorAttribute;
using E = layout::LinearExpression;
using R = layout::ConstraintRelation;

int main() {
  return test::run([] {
    ui::UIRoot root;
    auto owner = std::make_unique<ui::ConstraintLayout>();
    auto *node = owner.get();
    auto &a = node->append(
        "a", std::make_unique<ui::Rectangle>(ui::RectangleProps{}));
    auto &b = node->append(
        "b", std::make_unique<ui::Rectangle>(ui::RectangleProps{}));
    auto anchor = [](const char *key, A attribute) {
      return E{layout::LayoutAnchor{key, attribute}};
    };
    auto parent = [](A attribute) {
      return E{layout::LayoutAnchor::parent(attribute)};
    };
    node->setConstraints(
        {{anchor("a", A::Start), R::Equal, 10},
         {anchor("a", A::Width), R::Equal, 40},
         {anchor("a", A::Height), R::Equal, 20},
         {anchor("b", A::Start), R::Equal, anchor("a", A::End) + 5},
         {anchor("b", A::End), R::Equal, parent(A::End) - 10},
         {anchor("b", A::Height), R::Equal, anchor("a", A::Height)}});
    root.setContent(std::move(owner));
    root.flushLayout({200, 100});
    test::require(a.bounds().x() == 10 && b.bounds().x() == 55 &&
                      b.bounds().w() == 135,
                  "sibling and parent anchors resolve");
    test::rejects(
        [&] { node->addConstraint({anchor("a", A::Width), R::Equal, 41}); },
        "contradictory required candidate rejected");
    test::rejects(
        [&] {
          node->addConstraint({anchor("missing", A::Width), R::Equal, 41});
        },
        "missing local key rejected");
    node->addConstraint({anchor("b", A::Width), R::Equal, 10,
                         layout::ConstraintStrength::Weak});
    root.flushLayout({240, 100});
    test::require(b.bounds().w() == 175,
                  "required relation wins over weak preference after resize");
    test::rejects<std::logic_error>(
        [&] { node->takeChild("a"); },
        "live constraints prevent stale child references");
    node->setConstraints({{anchor("a", A::Width), R::GreaterEqual, 30},
                          {anchor("a", A::Width), R::LessEqual, 50},
                          {anchor("a", A::Width), R::Equal, 100,
                           layout::ConstraintStrength::Strong}});
    root.flushLayout({200, 100});
    test::require(a.bounds().w() == 50, "inequality clamps preference");
    node->setConstraints({});
    test::require(node->takeChild("a") != nullptr,
                  "child detachable after clearing relations");
    test::rejects([&] { node->setProps({0}); }, "iteration budget validated");
    node->append("wrapped",
                 std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
                     .measure = [](ui::MeasureContext &,
                                   const layout::SizeConstraints &offered) {
                       return layout::MeasureResult{
                           {offered.width.maximum.value_or(100.0f),
                            offered.width.maximum ? 40.0f : 10.0f}};
                     }}));
    node->setConstraints({{anchor("wrapped", A::Width), R::Equal, 40}});
    root.flushLayout({200, 100});
    test::require(node->children().back()->bounds().h() == 40,
                  "width-dependent height is remeasured");
    node->setProps({1});
    test::rejects<std::runtime_error>(
        [&] { root.flushLayout({200, 100}); },
        "nonconvergence fails within configured budget");
    node->setProps({});
    root.flushLayout({200, 100});
    {
      auto baselines = std::make_unique<ui::ConstraintLayout>();
      baselines->append(
          "label",
          std::make_unique<ui::CustomView>(
              ui::CustomViewCallbacks{.measure =
                                          [](ui::MeasureContext &,
                                             const layout::SizeConstraints &) {
                                            return layout::MeasureResult{
                                                {30, 20}, 12.0f, 12.0f};
                                          }},
              layout::BoxProps{.height = layout::SizeRule::fixed(20)}));
      baselines->setConstraints({{anchor("label", A::Top), R::Equal, 0},
                                 {anchor("label", A::Baseline), R::Equal, 12}});
      root.setContent(std::move(baselines));
      root.flushLayout({100, 100});
      test::require(root.content()->children()[0]->bounds().y() == 0,
                    "baseline constraints may be authored before measurement");
    }
    {
      auto owner = std::make_unique<ui::ConstraintLayout>();
      owner->append("rtl",
                    std::make_unique<ui::Rectangle>(ui::RectangleProps{}));
      owner->setConstraints(
          {{anchor("rtl", A::End), R::Equal, anchor("rtl", A::Start) - 30}});
      root.setContent(std::move(owner));
      root.flushLayout(ui::LayoutEnvironment{
          .viewport = {100, 100},
          .direction = layout::LayoutDirection::RightToLeft});
      test::require(
          root.content()->children()[0]->bounds().w() == 30,
          "RTL-only constraints are not rejected using LTR assumptions");
    }
    {
      auto owner = std::make_unique<ui::ConstraintLayout>();
      owner->append(
          "label",
          std::make_unique<ui::CustomView>(
              ui::CustomViewCallbacks{
                  .measure =
                      [](ui::MeasureContext &,
                         const layout::SizeConstraints &offered) {
                        const float baseline =
                            offered.width.maximum ? 12.0f : 8.0f;
                        return layout::MeasureResult{
                            {30, 20}, baseline, baseline};
                      }},
              layout::BoxProps{.height = layout::SizeRule::fixed(20)}));
      owner->setConstraints({{anchor("label", A::Top), R::Equal, 0},
                             {anchor("label", A::Width), R::Equal, 30},
                             {anchor("label", A::Baseline), R::Equal, 12}});
      root.setContent(std::move(owner));
      root.flushLayout({100, 100});
      test::require(
          root.content()->children()[0]->bounds().y() == 0,
          "baseline reflow is measured before rejecting required geometry");
      auto *node = static_cast<ui::ConstraintLayout *>(root.content());
      node->setConstraints({{anchor("label", A::Top), R::Equal, 0},
                            {anchor("label", A::Baseline), R::Equal, 15}});
      test::rejects([&] { root.flushLayout({100, 100}); },
                    "provisional baseline never commits an unsatisfied "
                    "measured baseline");
    }
  });
}
