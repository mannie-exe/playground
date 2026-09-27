#include <memory>
#include <vector>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/controls/Button.hpp>

using namespace playground;

namespace {
struct Draw {
  math::Rect bounds;
  math::ColorRGBA8 color;
};

class Painter final : public rendering::PaintContext {
  struct State {
    math::Vec2f translation{};
    math::Rect clip{math::rect(0, 0, 1000, 1000)};
  } _state;

  std::vector<State> _stack;

  math::Rect translated(math::Rect rect) const {
    rect.position += _state.translation;
    return rect;
  }

public:
  std::vector<Draw> draws;

  void save() override { _stack.push_back(_state); }

  void restore() noexcept override {
    _state = _stack.back();
    _stack.pop_back();
  }

  void translate(math::Vec2f delta) override { _state.translation += delta; }

  void clip(math::Rect rect) override {
    _state.clip = math::intersect(_state.clip, translated(rect));
  }

  void fill(math::Rect rect, math::ColorRGBA8 color) override {
    draws.push_back({math::intersect(_state.clip, translated(rect)), color});
  }
};
} // namespace

int main() {
  return test::run([] {
    ui::UIRoot root;
    const math::ColorRGBA8 ink{200, 10, 20, 255};
    auto button = std::make_unique<ui::Button>(
        nullptr, ui::ButtonProps{.normal = ink, .useTheme = false},
        layout::BoxProps{.height = layout::SizeRule::fixed(300)});
    int activated{};
    auto connection = button->onActivate([&] { ++activated; });
    auto *content = button.get();
    auto scroll = std::make_unique<ui::ScrollView>(std::move(button));
    auto *view = scroll.get();
    root.setContent(std::move(scroll));
    root.flushLayout({100, 80});
    test::require(view->viewportExtent() == math::Size2{92, 80},
                  "vertical gutter reserves eight logical units");
    test::require(content->bounds().w() == 92,
                  "non-scrolling axis remeasures to usable width");
    Painter painter;
    root.render(painter);
    test::require(
        painter.draws.size() == 3 && painter.draws[0].color == ink &&
            painter.draws[0].bounds == math::rect(0, 0, 92, 80) &&
            painter.draws[1].bounds == math::rect(92, 0, 8, 80),
        "content is clipped first, then gutter and thumb paint separately");
    auto hit = root.hitTest({96, 70});
    test::require(hit && hit->target.get() == view,
                  "whole track wins hit testing, not only thumb");
    hit = root.hitTest({91, 70});
    test::require(hit && hit->target.get() == content,
                  "content remains clickable to usable edge");
    auto send = [&](ui::EventType type, math::Point2 position) {
      ui::UIEvent e{
          .type = type, .position = position, .pointer = 1, .button = 1};
      root.dispatch(e);
      return e.handled;
    };
    test::require(send(ui::EventType::PointerDown, {96, 70}) &&
                      view->offset().y > 0,
                  "track click scrolls rather than activating content");
    send(ui::EventType::PointerUp, {96, 70});
    test::require(activated == 0,
                  "complete track gesture cannot activate underlying row");
    root.flushLayout({100, 80});
    send(ui::EventType::PointerDown, {96, 65});
    send(ui::EventType::InputCancel, {});
    const auto offset = view->offset();
    send(ui::EventType::PointerMove, {96, 0});
    test::require(view->offset() == offset,
                  "input cancellation ends scrollbar drag");
    root.flushLayout({100, 400});
    test::require(
        view->viewportExtent() == math::Size2{100, 400} &&
            content->bounds().w() == 100 && view->offset() == math::Vec2f{},
        "expansion removes Auto gutter and restores full content width");
    view->applyPatch({.scrollbar = Patch<ui::ScrollbarPolicy>::set(
                          ui::ScrollbarPolicy::Always)});
    root.flushLayout({100, 400});
    test::require(view->viewportExtent().width == 92,
                  "Always reserves gutter without overflow");
    view->applyPatch({.scrollbarThickness = Patch<float>::set(0)});
    root.flushLayout({100, 80});
    test::require(view->viewportExtent().width == 100,
                  "zero-thickness bars reserve nothing");

    view->setProps({.axes = ui::ScrollAxes::Both});
    view->setChild(std::make_unique<ui::Box>(
        layout::BoxProps{.width = layout::SizeRule::fixed(100),
                         .height = layout::SizeRule::fixed(81)}));
    root.flushLayout({100, 80});
    test::require(view->viewportExtent() == math::Size2{92, 72},
                  "one gutter can induce the second scrollbar");
    hit = root.hitTest({96, 76});
    test::require(hit && hit->target.get() == view,
                  "scrollbar corner never hits content");
    root.flushLayout({2, 2});
    test::require(view->viewportExtent() == math::Size2{},
                  "gutters clamp in tiny viewports");
    painter.draws.clear();
    root.render(painter);
    for (const auto &draw : painter.draws)
      test::require(math::isFinite(draw.bounds) &&
                        math::isNonNegative(draw.bounds.size),
                    "tiny viewport produces valid paint geometry");
  });
}
