#include <memory>
#include <vector>

#include <support/Test.hpp>
#include <ui/Builders.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>

using namespace playground;

class Paint final : public rendering::PaintContext {
public:
  int depth{};

  void save() override { ++depth; }

  void restore() noexcept override { --depth; }

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect, math::ColorRGBA8) override {}
};

int main() {
  return test::run([] {
    ui::UIRoot root;
    Paint paint;
    auto component = std::make_unique<ui::Component>();
    auto *c = component.get();
    c->replaceContent(std::make_unique<ui::Box>());
    root.setContent(std::move(component));
    auto old = c->children().front()->handle();
    test::rejects([&] { c->replaceContent(nullptr); },
                  "null replacement rejected");
    test::require(bool(old),
                  "failed component replacement preserves old content");
    c->replaceContent(std::make_unique<ui::Box>());
    test::require(!old, "successful component replacement expires old handle");

    auto stack = ui::make<ui::HStack>();
    auto *spacer = &stack->append(ui::make<ui::Spacer>());
    stack->append(ui::make<ui::Box>(
        layout::BoxProps{.width = layout::SizeRule::fixed(20)}));
    root.setContent(std::move(stack));
    root.flushLayout({100, 30});
    test::require(spacer->bounds().w() == 80,
                  "implicit Spacer grow takes leftover main space");

    std::vector<int> order;
    auto overlay = ui::make<ui::ZStack>(
        ui::ZStackProps{.childrenAlignment = layout::Alignment::stretch()});
    for (int n : {1, 2}) {
      auto view = ui::make<ui::CustomView>(ui::CustomViewCallbacks{
          .measure =
              [](ui::MeasureContext &, const layout::SizeConstraints &) {
                return layout::MeasureResult{{20, 10}};
              },
          .paint = [&, n](rendering::PaintContext &) { order.push_back(n); }});
      view->setHitTestPolicy(ui::HitTestPolicy::Self);
      overlay->append(std::move(view));
    }
    auto *top = overlay->children().back().get();
    root.setContent(std::move(overlay));
    root.flushLayout({100, 30});
    root.render(paint);
    test::require(order == std::vector{1, 2} && paint.depth == 0,
                  "ZStack paints in source order");
    test::require(root.hitTest({5, 5})->target.get() == top,
                  "ZStack hits reverse paint order");

    for (auto visibility : {ui::Visibility::Visible, ui::Visibility::Hidden,
                            ui::Visibility::Collapsed})
      for (auto policy :
           {ui::HitTestPolicy::None, ui::HitTestPolicy::ChildrenOnly,
            ui::HitTestPolicy::Self, ui::HitTestPolicy::SelfAndChildren}) {
        order.clear();
        top->setVisibility(visibility);
        top->setHitTestPolicy(policy);
        root.flushLayout({100, 30});
        root.render(paint);
        test::require((order.size() == 2) ==
                          (visibility == ui::Visibility::Visible),
                      "visibility gates paint");
        const auto hit = root.hitTest({5, 5});
        const bool expectsTop = visibility == ui::Visibility::Visible &&
                                (policy == ui::HitTestPolicy::Self ||
                                 policy == ui::HitTestPolicy::SelfAndChildren);
        test::require(hit && (hit->target.get() == top) == expectsTop,
                      "visibility and hit policy variants route correctly");
      }

    auto view = ui::make<ui::CustomView>();
    auto *viewPtr = view.get();
    view->setHitTestPolicy(ui::HitTestPolicy::Self);
    auto transformed = ui::make<ui::Transform>(
        std::move(view),
        ui::VisualProps{.transform = math::Transform2D::translation({10, 0})});
    transformed->setContentAlignment(layout::Alignment::stretch());
    root.setContent(std::move(transformed));
    root.flushLayout({100, 30});
    test::require(!root.hitTest({5, 5}) &&
                      root.hitTest({15, 5})->target.get() == viewPtr,
                  "transform affects picking without changing local bounds");
  });
}
