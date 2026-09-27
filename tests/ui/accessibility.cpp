#include <memory>
#include <stdexcept>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/controls/Composite.hpp>
#include <ui/controls/Slider.hpp>

using namespace playground;

int main() {
  return test::run([] {
    ui::UIRoot root;
    auto layout = std::make_unique<ui::VStack>();
    auto *column = layout.get();
    auto button = std::make_unique<ui::Button>();
    auto *raw = button.get();
    button->setSemanticProps({.name = "Launch"});
    int activations{};
    auto connection = button->onActivate([&] { ++activations; });
    column->append(std::move(button));
    auto slider = std::make_unique<ui::Slider>();
    auto *range = slider.get();
    column->append(std::move(slider));
    auto modal = std::make_unique<ui::Dialog>(
        std::make_unique<ui::Button>(), ui::DialogProps{.name = "Confirm"});
    auto *dialog = modal.get();
    column->append(std::move(modal));
    root.setContent(std::move(layout));
    root.flushLayout({320, 240});
    test::require(root.performAction(raw->id(), ui::Activate{},
                                     ui::ActionSource::Assistive) ==
                          ui::ActionResult::Applied &&
                      activations == 1,
                  "native activation follows control action");
    raw->setEnabled(false);
    test::require(root.performAction(raw->id(), ui::Activate{},
                                     ui::ActionSource::Assistive) ==
                          ui::ActionResult::Unavailable &&
                      activations == 1,
                  "disabled actions rejected");
    auto snapshot = root.semanticSnapshot();
    test::require(!snapshot.nodes.front().state.description.enabled &&
                      snapshot.nodes.front().state.actions.empty(),
                  "disabled semantics derived from control");
    raw->setEnabled(true);
    raw->setHitTestPolicy(ui::HitTestPolicy::None);
    root.requestFocus(raw->id());
    test::require(raw->hasFocus(),
                  "pointer hit testing independent of keyboard focus");
    auto inputs = raw->inputProps();
    inputs.neighbors.next = range->id();
    raw->setInputProps(inputs);
    root.focusNext();
    test::require(range->hasFocus(), "explicit sequential neighbor");
    root.requestFocus(raw->id());
    dialog->applyPatch({.open = Patch<bool>::set(true)});
    root.flushLayout({320, 240});
    test::require(root.performAction(raw->id(), ui::Activate{},
                                     ui::ActionSource::Assistive) ==
                      ui::ActionResult::Unavailable,
                  "modal blocks background actions");
    snapshot = root.semanticSnapshot();
    test::require(snapshot.nodes.front().id == dialog->id(),
                  "modal semantic snapshot excludes background");
    ui::UIEvent escape{.type = ui::EventType::KeyDown,
                       .logicalKey = ui::Key::Escape};
    root.dispatch(escape);
    test::require(!dialog->props().open && escape.handled,
                  "escape dismisses dialog");
    root.flushLayout({320, 240});
    root.performAction(range->id(), ui::SetValue{3},
                       ui::ActionSource::Assistive);
    test::require(raw->hasFocus(), "opener focus restored");
    const auto stale = raw->id();
    column->remove(stale);
    test::require(root.performAction(stale, ui::Activate{},
                                     ui::ActionSource::Assistive) ==
                      ui::ActionResult::Stale,
                  "removed identities reject queued actions");
    test::require(range->props().range.value == 3, "range mutation delivered");
    test::require(root.performAction(range->id(), ui::SetValue{101},
                                     ui::ActionSource::Assistive) ==
                      ui::ActionResult::Unavailable,
                  "invalid range rejected");
    auto p = range->semanticProps();
    p.exposure = ui::SemanticExposure::HiddenSubtree;
    range->setSemanticProps(p);
    snapshot = root.semanticSnapshot();
    for (const auto &node : snapshot.nodes)
      test::require(node.id != range->id(), "hidden semantics omitted");
  });
}
