#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/controls/Button.hpp>

int main() {
  return playground::test::run([] {
    using namespace playground;
    auto button = std::make_unique<ui::Button>();
    auto *buttonPtr = button.get();
    button->setVisualProps(
        {.transform = math::Transform2D::translation({-10, -10})});
    auto clip = std::make_unique<ui::Clip>(
        std::move(button), std::nullopt,
        layout::BoxProps{.padding = math::Insets::all(10)});
    clip->setContentAlignment(layout::Alignment::stretch());
    auto *clipPtr = clip.get();
    ui::UIRoot root;
    root.setContent(std::move(clip));
    root.flushLayout({100, 100});
    test::require(clipPtr->clipBounds() == math::rect(10, 10, 80, 80),
                  "default clip is content box");
    test::require(!root.hitTest({5, 5}),
                  "padding outside clip cannot hit an overflowing child");
    auto inside = root.hitTest({15, 15});
    test::require(inside && inside->target.get() == buttonPtr,
                  "inside content can hit child");
    clipPtr->setClipRect(math::rect(0, 0, 100, 100));
    test::require(root.hitTest({5, 5}).has_value(),
                  "explicit clip override uses local coordinates");
    buttonPtr->setVisualProps(
        {.transform = math::Transform2D::scaling({0, 1})});
    test::require(!root.hitTest({15, 15}), "singular child is not hittable");
    const auto &diagnostics = root.diagnostics().entries();
    test::require(!diagnostics.empty() &&
                      diagnostics.back().issue ==
                          ui::LayoutIssue::NonInvertibleTransform,
                  "singular hit transform produces a diagnostic");
  });
}
