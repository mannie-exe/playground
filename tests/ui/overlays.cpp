#include <memory>
#include <string>
#include <vector>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/ZStack.hpp>
#include <ui/controls/Composite.hpp>

using namespace playground;

namespace {
auto label() {
  return std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
      .measure = [](ui::MeasureContext &, const layout::SizeConstraints &) {
        return layout::MeasureResult{{80, 20}, {}, {}};
      }});
}

std::vector<ui::ChoiceItem> choices() {
  std::vector<ui::ChoiceItem> result;
  result.push_back({"one", "One", label()});
  result.push_back({"off", "Disabled", label(), false});
  result.push_back({"two", "Two", label()});
  return result;
}
} // namespace

int main() {
  return test::run([] {
    ui::UIRoot root;
    auto layers = std::make_unique<ui::ZStack>(
        ui::ZStackProps{layout::Alignment::stretch()});
    auto behind = std::make_unique<ui::Button>();
    auto *background = behind.get();
    int clicks{};
    auto clicked = behind->onActivate([&] { ++clicks; });
    layers->append(std::move(behind));
    auto select = std::make_unique<ui::Select>(
        label(), choices(), ui::SelectionProps{.selected = "one"});
    auto *picker = select.get();
    select->setBoxProps({.width = layout::SizeRule::fixed(140)});
    select->setClip(true);
    layers->append(std::move(select),
                   {.margin = {20, 20, 0, 0},
                    .alignmentOverride = layout::Alignment{
                        layout::Align::Start, layout::Align::Start}});
    root.setContent(std::move(layers));
    root.flushLayout({400, 300});
    const auto closed = picker->bounds();
    const auto open = [&] {
      picker->setExpanded(true);
      root.flushLayout({400, 300});
    };
    const auto key = [&](ui::Key key) {
      ui::UIEvent e{.type = ui::EventType::KeyDown, .logicalKey = key};
      root.dispatch(e);
    };
    const auto pointer = [&](ui::EventType type, math::Point2 point) {
      ui::UIEvent e{.type = type, .position = point, .pointer = 7, .button = 1};
      root.dispatch(e);
      return e;
    };
    open();
    auto *popup = dynamic_cast<ui::Popup *>(picker->children()[1].get());
    test::require(popup && picker->bounds() == closed,
                  "opening popup does not occupy parent layout");
    test::require(popup->bounds().y() >= closed.bottom(),
                  "popup positioned below anchor in root coordinates");
    auto *scroll = dynamic_cast<ui::ScrollView *>(popup->children()[0].get());
    test::require(scroll && scroll->bounds().w() == popup->bounds().w(),
                  "popup scroll view fills the visible surface");
    auto *list = scroll->child();
    for (const auto &row : list->children())
      test::require(
          row->bounds().w() == scroll->viewportExtent().width,
          "options fill the popup's usable width, not just their labels");
    auto hit = root.hitTest({popup->bounds().x() + 5, popup->bounds().y() + 5});
    test::require(hit && hit->target.get() != background,
                  "portal escapes owner's clip for hit testing");
    key(ui::Key::Down);
    test::require(picker->selectionProps().selected == "one",
                  "arrow preview does not commit");
    bool active{};
    for (const auto &node : root.semanticSnapshot().nodes)
      active |= node.state.activeDescendant.has_value();
    test::require(active,
                  "active option is exposed without committing selection");
    key(ui::Key::Enter);
    test::require(!picker->isExpanded() &&
                      picker->selectionProps().selected == "two",
                  "Enter commits enabled preview");
    open();
    const math::Point2 firstOption{popup->bounds().right() - 2,
                                   popup->bounds().y() + 15};
    pointer(ui::EventType::PointerDown, firstOption);
    pointer(ui::EventType::PointerUp, firstOption);
    test::require(!picker->isExpanded() &&
                      picker->selectionProps().selected == "one",
                  "pointer commits option through portal route");
    picker->performAction(ui::SelectItem{"two"}, ui::ActionSource::Program);
    open();
    key(ui::Key::Home);
    key(ui::Key::Escape);
    test::require(!picker->isExpanded() &&
                      picker->selectionProps().selected == "two" &&
                      picker->children()[0]->hasFocus(),
                  "Escape cancels preview and restores anchor");
    open();
    test::require(
        pointer(ui::EventType::PointerDown, {350, 250}).propagationStopped,
        "outside press consumed");
    test::require(
        pointer(ui::EventType::PointerUp, {350, 250}).propagationStopped &&
            clicks == 0,
        "outside release cannot click through");
    pointer(ui::EventType::PointerDown, {350, 250});
    pointer(ui::EventType::PointerUp, {350, 250});
    test::require(clicks == 1, "next independent click still works");
    pointer(ui::EventType::PointerDown, {350, 250});
    test::require(background->isPressed(),
                  "background capture armed before programmatic popup");
    open();
    test::require(!background->isPressed(),
                  "opening popup cancels an unrelated held press");
    pointer(ui::EventType::PointerUp, {350, 250});
    test::require(clicks == 1, "canceled press cannot activate behind popup");
    open();
    key(ui::Key::Tab);
    test::require(!picker->isExpanded(), "Tab closes transient picker");
    open();
    ui::UIEvent lost{.type = ui::EventType::FocusLost};
    root.dispatch(lost);
    test::require(!picker->isExpanded(), "window focus loss closes picker");
    open();
    picker->setVisibility(ui::Visibility::Collapsed);
    root.flushLayout({400, 300});
    test::require(!picker->isExpanded(), "hidden owner closes popup");

    auto dialog = std::make_unique<ui::Dialog>(
        std::make_unique<ui::Button>(label()), ui::DialogProps{.open = true},
        layout::BoxProps{.padding = math::Insets::all(10)});
    auto *modal = dialog.get();
    root.setContent(std::move(dialog));
    root.flushLayout({400, 300});
    test::require(modal->bounds().w() < 400 && modal->bounds().x() > 0,
                  "root dialog centers its preferred size");
    key(ui::Key::Escape);
    test::require(!modal->props().open,
                  "modal Escape closes through portal routing");

    auto nested = std::make_unique<ui::Select>(
        label(), choices(), ui::SelectionProps{.selected = "one"});
    auto *inside = nested.get();
    auto enclosing = std::make_unique<ui::Dialog>(
        std::move(nested), ui::DialogProps{.open = true},
        layout::BoxProps{.width = layout::SizeRule::fixed(200)});
    auto *enclosingPtr = enclosing.get();
    root.setContent(std::move(enclosing));
    root.flushLayout({400, 300});
    inside->setExpanded(true);
    root.flushLayout({400, 300});
    test::require(inside->isExpanded(),
                  "popup belongs to enclosing modal scope");
    key(ui::Key::Escape);
    test::require(!inside->isExpanded() && enclosingPtr->props().open,
                  "Escape closes nested popup before modal");
    key(ui::Key::Escape);
    test::require(!enclosingPtr->props().open,
                  "next Escape closes enclosing modal");

    ui::Popup placement{label(), {.open = true}};
    ui::Button anchor{label(),
                      {},
                      {.width = layout::SizeRule::fixed(100),
                       .height = layout::SizeRule::fixed(30)}};
    ui::ArrangeContext context;
    anchor.arrange(context, math::rect(180, 170, 100, 30));
    placement.present(context, math::rect(0, 0, 300, 210), &anchor);
    test::require(placement.bounds().bottom() <= anchor.bounds().y(),
                  "bottom edge chooses above fallback");
    test::rejects(
        [&] {
          placement.applyPopupPatch({.maximumHeight = Patch<float>::set(-1)});
        },
        "invalid popup patch rejected");
    test::require(placement.popupProps().maximumHeight == 320,
                  "rejected patch preserves props");
    placement.present(context, math::rect(0, 0, 30, 15), &anchor);
    test::require(placement.bounds().x() >= 0 &&
                      placement.bounds().right() <= 30 &&
                      placement.bounds().bottom() <= 15,
                  "tiny viewport clamps popup");

    std::vector<ui::ChoiceItem> many;
    for (int i = 0; i < 20; ++i)
      many.push_back({std::to_string(i), "Option", label()});
    auto longSelect = std::make_unique<ui::Select>(
        label(), std::move(many), ui::SelectionProps{.selected = "0"});
    auto *longPicker = longSelect.get();
    root.setContent(std::move(longSelect));
    root.flushLayout({400, 600});
    longPicker->setExpanded(true);
    root.flushLayout({400, 600});
    auto &longPopup = *longPicker->children()[1];
    auto &longScroll = dynamic_cast<ui::ScrollView &>(*longPopup.children()[0]);
    test::require(longScroll.viewportExtent().width ==
                      longPopup.bounds().w() - 8,
                  "long dropdown reserves its own scrollbar gutter");
    for (const auto &row : longScroll.child()->children())
      test::require(row->bounds().w() == longScroll.viewportExtent().width,
                    "long dropdown rows fill usable width");
    const math::Point2 rail{longPopup.bounds().right() - 2,
                            longPopup.bounds().bottom() - 20};
    pointer(ui::EventType::PointerDown, rail);
    pointer(ui::EventType::PointerUp, rail);
    test::require(longPicker->isExpanded() &&
                      longPicker->selectionProps().selected == "0" &&
                      longScroll.offset().y > 0,
                  "popup scrollbar scrolls without selecting or dismissing");
    longScroll.setOffset({});
    root.flushLayout({400, 600});
    const math::Point2 rowEdge{longPopup.bounds().x() +
                                   longScroll.viewportExtent().width - 1,
                               longPopup.bounds().y() + 55};
    pointer(ui::EventType::PointerDown, rowEdge);
    pointer(ui::EventType::PointerUp, rowEdge);
    test::require(
        !longPicker->isExpanded() &&
            longPicker->selectionProps().selected == "1",
        "far-right row click selects beside, not underneath, scrollbar");
  });
}
