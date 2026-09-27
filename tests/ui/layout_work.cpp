#include <memory>
#include <stdexcept>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/AdaptiveStack.hpp>
#include <ui/collections/VirtualList.hpp>
#include <ui/containers/Boundaries.hpp>
#include <ui/containers/ZStack.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

class Probe final : public ui::Node {
public:
  int arrangements{};
  bool fail{}, mutate{};
  math::Size2 desired{20, 20};

protected:
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &) override {
    return {desired};
  }

  void arrangeChildren(ui::ArrangeContext &, math::Rect) override {
    ++arrangements;
    if (fail)
      throw std::runtime_error("injected layout failure");
    if (mutate) {
      mutate = false;
      invalidateLayout();
    }
  }
};

class Source final : public ui::CollectionSource {
public:
  std::size_t size() const override { return 100; }

  ui::ItemKey keyAt(std::size_t i) const override { return std::to_string(i); }
};

int main() {
  return test::run([] {
    ui::UIRoot root;
    auto leaf = std::make_unique<Probe>();
    auto *probe = leaf.get();
    std::unique_ptr<ui::Node> tree = std::move(leaf);
    constexpr int depth = 32;
    for (int i = 0; i < depth; ++i) {
      auto box = std::make_unique<ui::Box>();
      box->setChild(std::move(tree));
      tree = std::move(box);
    }
    root.setContent(std::move(tree));
    root.flushLayout({100, 100});
    const auto visits = root.stats().invalidationVisits;
    probe->invalidateLayout();
    test::require(root.stats().invalidationVisits - visits == depth + 1,
                  "one visit per ancestor rather than triangular walks");

    auto group = std::make_unique<ui::ZStack>();
    auto child = std::make_unique<Probe>();
    probe = child.get();
    auto boundary = std::make_unique<ui::LayoutBoundary>(math::Size2{40, 40},
                                                         std::move(child));
    auto *island = boundary.get();
    auto sibling = std::make_unique<Probe>();
    auto *other = sibling.get();
    group->append(std::move(boundary));
    group->append(std::move(sibling));
    root.setContent(std::move(group));
    root.flushLayout({100, 100});
    const int before = other->arrangements;
    probe->desired = {30, 30};
    probe->invalidateLayout();
    root.flushLayout({100, 100});
    test::require(other->arrangements == before,
                  "boundary does not rearrange unrelated sibling");
    test::require(probe->bounds().w() == 40,
                  "boundary arranges child inside its assigned extent");
    probe->fail = true;
    probe->invalidateLayout();
    test::rejects<std::runtime_error>([&] { root.flushLayout({100, 100}); },
                                      "layout failure propagates");
    probe->fail = false;
    root.flushLayout({100, 100});
    test::require(probe->isArranged(), "failed boundary work is retried");
    probe->mutate = true;
    probe->invalidateLayout();
    root.flushLayout({100, 100});
    const int mutated = probe->arrangements;
    root.flushLayout({100, 100});
    test::require(probe->arrangements > mutated,
                  "mutation raised during arrange survives the pass");
    island->setExtent({60, 60});
    auto invalidBoundary = island->settings();
    invalidBoundary.box.width = layout::SizeRule::content();
    test::rejects(
        [&] { island->setSettings(invalidBoundary); },
        "common settings cannot invalidate the fixed-extent contract");
    root.flushLayout({100, 100});
    test::require(probe->bounds().w() == 60,
                  "authored boundary size propagates outside");
    probe->invalidateLayout();
    root.setContent(std::make_unique<Probe>());
    root.flushLayout({100, 100});
    test::require(root.content()->isArranged(),
                  "stale queued boundary is harmless after detach");

    ui::Rectangle rectangle{{.fill = {1, 2, 3, 255}}};
    ui::UIWorkStats work;
    ui::MeasureContext context{.stats = &work};
    rectangle.measure(context, layout::SizeConstraints::tight({10, 20}));
    rectangle.measure(context, layout::SizeConstraints::tight({20, 40}));
    rectangle.measure(context, layout::SizeConstraints::tight({10, 20}));
    test::require(work.measured == 2 && work.measureCacheHits == 1,
                  "pure leaf retains two exact offers");

    auto source = std::make_shared<Source>();
    ui::VirtualList list{
        source,
        {.create = [](const auto &) { return std::make_unique<Probe>(); }},
        {.itemExtent = 20, .overscan = 0}};
    list.measure(context, layout::SizeConstraints::tight({100, 40}));
    const auto count = list.children().size();
    list.measure(context, layout::SizeConstraints::tight({100, 160}));
    test::require(list.children().size() > count,
                  "larger offer realizes more children");
    list.measure(context, layout::SizeConstraints::tight({100, 40}));
    test::require(list.children().size() == count,
                  "A/B/A restores collection realization, not just its size");

    ui::AdaptiveStack adaptive{
        {.breakpoints = {.rules = {{.availableSpace = {.minimum = {100, 0}},
                                    .mode = layout::Axis::Horizontal}},
                         .fallback = layout::Axis::Vertical}}};
    adaptive.append(std::make_unique<Probe>());
    adaptive.measure(context, layout::SizeConstraints::tight({200, 100}));
    adaptive.measure(context, layout::SizeConstraints::tight({50, 100}));
    test::require(adaptive.selectedAxis() == layout::Axis::Vertical,
                  "narrow offer selects vertical axis");
    adaptive.measure(context, layout::SizeConstraints::tight({200, 100}));
    test::require(adaptive.selectedAxis() == layout::Axis::Horizontal,
                  "A/B/A adaptive state is restored");

    rectangle.arrange(context, math::rect(0, 0, 10, 20));
    const auto arrangements = work.arranged;
    rectangle.arrange(context, math::rect(0, 0, 10, 20));
    test::require(work.arranged == arrangements && work.arrangeSkips == 1,
                  "identical successful arrangement reuses geometry");
    context.pixelScale = {2, 2};
    rectangle.arrange(context, math::rect(0, 0, 10, 20));
    test::require(work.arranged == arrangements + 1,
                  "density changes invalidate arrangement reuse");
  });
}
