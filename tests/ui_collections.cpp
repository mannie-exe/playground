#include <iostream>
#include <stdexcept>

#include <ui/UIRoot.hpp>
#include <ui/collections/AdaptiveStack.hpp>
#include <ui/collections/Repeat.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/collections/VirtualGrid.hpp>
#include <ui/collections/VirtualList.hpp>
#include <ui/containers/Box.hpp>

namespace ui = playground::ui;
namespace math = playground::math;
namespace layout = playground::layout;
static void check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
class Source : public ui::CollectionSource {
public:
  std::size_t count{10000};
  bool reversed{};
  std::size_t size() const override { return count; }
  ui::ItemKey keyAt(std::size_t i) const override {
    return std::to_string(reversed ? count - 1 - i : i);
  }
};
static std::unique_ptr<ui::Node> leaf() {
  return std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
      .measure = [](ui::MeasureContext &, const layout::SizeConstraints &) {
        return layout::MeasureResult{{20, 20}};
      }});
}
int main() {
  try {
    ui::detail::ExtentIndex extents{std::vector<double>{10, 20, 30}};
    check(extents.itemAt(10) == 1 && extents.prefix(2) == 30,
          "Fenwick boundary search");
    extents.set(1, 40);
    check(extents.total() == 80, "Fenwick update");
    auto source = std::make_shared<Source>();
    ui::ItemFactory factory{
        .create = [](const ui::ItemKey &) { return leaf(); }};
    ui::UIRoot root;
    auto list = std::make_unique<ui::VirtualList>(
        source, factory,
        ui::VirtualListProps{.itemExtent = 20, .overscan = 20});
    auto *v = list.get();
    root.setContent(std::move(list));
    root.flushLayout({200, 100});
    check(v->children().size() < 10 && v->visibleKeys().front() == "0",
          "virtual list bounded live nodes");
    v->scrollToKey("5000");
    root.flushLayout({200, 100});
    check(v->children().size() < 12, "distant virtual list scroll bounded");
    auto grid = std::make_unique<ui::VirtualGrid>(
        source, factory,
        ui::VirtualGridProps{
            .columns = 100, .cellExtent = {20, 20}, .overscan = 0});
    auto *g = grid.get();
    root.setContent(std::move(grid));
    root.flushLayout({100, 100});
    check(g->children().size() == 25, "virtual grid only visible cells");
    g->scrollToCell(10, 10);
    root.flushLayout({100, 100});
    check(g->visibleKeys().front() == "1010", "virtual grid indexed scroll");
    root.flushLayout(math::Size2{100, 100},
                     layout::LayoutDirection::RightToLeft);
    check(g->realized("1010")->bounds().x() == 80,
          "RTL virtual grid starts at right edge");
    source = std::make_shared<Source>();
    source->count = 3;
    auto horizontal = std::make_unique<ui::VirtualList>(
        source, factory,
        ui::VirtualListProps{
            .axis = layout::Axis::Horizontal, .itemExtent = 20, .overscan = 0});
    auto *horizontalPtr = horizontal.get();
    root.setContent(std::move(horizontal));
    root.flushLayout(math::Size2{40, 20}, layout::LayoutDirection::RightToLeft);
    check(horizontalPtr->realized("0")->bounds().x() == 20,
          "RTL virtual list starts at right edge");
    auto estimated = std::make_unique<ui::VirtualList>(
        source, factory,
        ui::VirtualListProps{.extentMode = ui::ItemExtentMode::Estimated,
                             .itemExtent = 10,
                             .overscan = 0});
    auto *estimatedPtr = estimated.get();
    root.setContent(std::move(estimated));
    root.flushLayout({100, 100});
    check(estimatedPtr->contentExtent() == 60,
          "estimated extents corrected from measured items");
    auto repeat = std::make_unique<ui::Repeat>(source, factory);
    auto *r = repeat.get();
    root.setContent(std::move(repeat));
    root.flushLayout({100, 100});
    auto handle = r->realized("0")->handle();
    source->reversed = true;
    r->applyCollectionChanges();
    root.flushLayout({100, 100});
    check(handle && r->realizedKeys()[2] == "0" &&
              r->realized("0") == handle.get(),
          "keyed reorder preserves identity");
    r->setArrangement(
        layout::GridProps{.columns = {layout::TrackSize::fraction(),
                                      layout::TrackSize::fraction()}});
    root.flushLayout({100, 100});
    check(handle && r->realized("0")->bounds().y() > 0,
          "repeat delegates grid layout without replacing items");
    r->setArrangement(layout::FlowProps{.itemGap = 4});
    root.flushLayout({35, 100});
    check(handle && r->realized("0")->bounds().y() > 0,
          "repeat delegates flow layout");
    auto adaptive = std::make_unique<ui::AdaptiveStack>(ui::AdaptiveStackProps{
        .breakpoints = {.rules = {{.availableSpace = {.minimum = {100, 0}},
                                   .mode = layout::Axis::Horizontal}},
                        .fallback = layout::Axis::Vertical}});
    auto *a = adaptive.get();
    auto *child = &a->append(leaf());
    a->append(leaf());
    root.setContent(std::move(adaptive));
    root.flushLayout({150, 100});
    auto id = child->id();
    check(a->selectedAxis() == layout::Axis::Horizontal, "wide adaptive row");
    root.flushLayout({80, 100});
    check(a->selectedAxis() == layout::Axis::Vertical && child->id() == id,
          "adaptive mode retains nodes");
    std::cout << "Collection tests passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
