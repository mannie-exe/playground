#include <memory>
#include <vector>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/controls/Choice.hpp>
#include <ui/controls/Slider.hpp>

using namespace playground;

namespace {
class ChoiceList final : public ui::ListBox {
public:
  using ListBox::highlight;
  using ListBox::ListBox;
};

struct Fill {
  math::Rect rect;
  math::ColorRGBA8 color;
  bool operator==(const Fill &) const = default;
};

class Painter final : public rendering::PaintContext {
public:
  std::vector<Fill> fills;

  void save() override {}

  void restore() noexcept override {}

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect r, math::ColorRGBA8 c) override {
    test::require(math::isFinite(r) && math::isNonNegative(r.size),
                  "valid control geometry");
    fills.push_back({r, c});
  }
};

auto label(float width) {
  return std::make_unique<ui::Box>(
      layout::BoxProps{.width = layout::SizeRule::fixed(width),
                       .height = layout::SizeRule::fixed(20)});
}
} // namespace

int main() {
  return test::run([] {
    for (auto scheme :
         {ui::ColorSchemePreference::Light, ui::ColorSchemePreference::Dark})
      for (auto contrast :
           {ui::ContrastPreference::Normal, ui::ContrastPreference::High}) {
        ui::UIRoot root;
        root.setTheme(ui::resolveTheme(scheme, contrast, {}));
        auto slider = std::make_unique<ui::Slider>();
        auto *control = slider.get();
        root.setContent(std::move(slider));
        root.flushLayout({160, 24});
        Painter painter;
        root.render(painter);
        const auto idle = painter.fills;
        auto send = [&](ui::EventType type, math::Point2 p,
                        std::uint64_t pointer = 1) {
          ui::UIEvent event{
              .type = type, .position = p, .pointer = pointer, .button = 1};
          root.dispatch(event);
        };
        send(ui::EventType::PointerMove, {80, 12});
        test::require(control->isHovered(), "pointer entry produces hover");
        painter.fills.clear();
        root.render(painter);
        test::require(painter.fills != idle,
                      "hover is visibly different in every palette");
        send(ui::EventType::PointerDown, {80, 12});
        test::require(control->isDragging(), "press captures slider");
        const double middle = control->props().range.value;
        send(ui::EventType::PointerDown, {150, 12}, 2);
        test::require(control->props().range.value == middle,
                      "second pointer cannot steal drag");
        send(ui::EventType::PointerCancel, {150, 12}, 2);
        test::require(control->isDragging(),
                      "unrelated cancellation preserves drag");
        send(ui::EventType::PointerMove, {160, 12});
        test::require(control->props().range.value ==
                          control->props().range.maximum,
                      "drag reaches upper endpoint");
        send(ui::EventType::PointerUp, {160, 12});
        test::require(!control->isDragging(), "release ends drag");
        send(ui::EventType::PointerDown, {8, 12});
        send(ui::EventType::InputCancel, {});
        test::require(!control->isDragging() && !control->isHovered(),
                      "input cancellation resets feedback");
        control->applyPatch({.readOnly = Patch<bool>::set(true)});
        send(ui::EventType::PointerMove, {70, 12});
        send(ui::EventType::PointerDown, {70, 12});
        test::require(!control->isDragging() && !control->isHovered(),
                      "read-only slider has no affordance for editing");
        control->applyPatch({.enabled = Patch<bool>::set(false),
                             .readOnly = Patch<bool>::set(false)});
        send(ui::EventType::PointerDown, {70, 12});
        test::require(!control->isDragging(),
                      "disabled slider does not capture");
        control->applyPatch(
            {.enabled = Patch<bool>::set(true),
             .axis = Patch<layout::Axis>::set(layout::Axis::Vertical)});
        root.flushLayout({24, 160});
        send(ui::EventType::PointerDown, {12, 8});
        test::require(control->props().range.value ==
                          control->props().range.maximum,
                      "vertical top is maximum");
        send(ui::EventType::PointerUp, {12, 152});
        test::require(control->props().range.value ==
                          control->props().range.minimum,
                      "vertical bottom is minimum");
        root.flushLayout({2, 2});
        painter.fills.clear();
        root.render(painter);

        std::vector<ui::ChoiceItem> items;
        items.push_back({"short", "Short", label(30)});
        items.push_back({"long", "Long", label(160)});
        items.push_back({"off", "Disabled", label(70), false});
        auto list = std::make_unique<ChoiceList>(std::move(items));
        auto *rows = list.get();
        root.setContent(std::move(list));
        root.flushLayout({300, 120});
        for (const auto &row : rows->children())
          test::require(row->bounds().w() == 300,
                        "option hit/background width comes from parent");
        painter.fills.clear();
        root.render(painter);
        test::require(
            painter.fills.empty(),
            "resting options have no per-button border or background");
        rows->performAction(ui::SelectItem{"short"},
                            ui::ActionSource::Keyboard);
        painter.fills.clear();
        root.render(painter);
        test::require(painter.fills.size() == 2,
                      "selection is a full-row fill and marker, not a border");
        test::require(painter.fills.front().rect.w() == 300,
                      "selected background spans the row");
        const auto selectedMarker = painter.fills.back().rect;
        rows->highlight("short");
        painter.fills.clear();
        root.render(painter);
        test::require(
            painter.fills.size() == 2 &&
                painter.fills.back().rect == selectedMarker &&
                painter.fills.back().color == rows->theme().focus,
            "active selected option has one same-sized focus-colored marker");
        rows->highlight("long");
        painter.fills.clear();
        root.render(painter);
        test::require(painter.fills.size() == 4 &&
                          painter.fills[1].rect == painter.fills[3].rect,
                      "different selected and active rows each have one "
                      "consistently sized marker");
        rows->highlight(std::nullopt);
        painter.fills.clear();
        root.render(painter);
        test::require(
            painter.fills.size() == 2 &&
                painter.fills.back().color == rows->theme().accent,
            "clearing active preview preserves committed selection styling");
      }
  });
}
