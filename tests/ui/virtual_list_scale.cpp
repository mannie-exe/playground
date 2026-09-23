#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/VirtualList.hpp>
#include <ui/containers/Box.hpp>

namespace {
class MillionItems final : public playground::ui::CollectionSource {
public:
  std::size_t size() const override { return 1'000'000; }
  playground::ui::ItemKey keyAt(std::size_t i) const override {
    return std::to_string(i);
  }
};
} // namespace
int main() {
  return playground::test::run([] {
    using namespace playground;
    auto list = std::make_unique<ui::VirtualList>(
        std::make_shared<MillionItems>(),
        ui::ItemFactory{.create =
                            [](const ui::ItemKey &) {
                              return std::make_unique<ui::Box>();
                            }},
        ui::VirtualListProps{.itemExtent = 20, .overscan = 20});
    auto *view = list.get();
    ui::UIRoot root;
    root.setContent(std::move(list));
    root.flushLayout({200, 100});
    test::require(view->children().size() < 10,
                  "million model entries do not create a million nodes");
    view->scrollToKey("500000");
    root.flushLayout({200, 100});
    test::require(view->children().size() < 12,
                  "distant scroll retains bounded working set");
  });
}
