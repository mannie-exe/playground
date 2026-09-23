#include <memory>
#include <string>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/Repeat.hpp>
#include <ui/collections/VirtualList.hpp>
#include <ui/controls/Button.hpp>
#include <vector>

using namespace playground;

struct Source final : ui::CollectionSource {
  std::vector<ui::ItemKey> keys{"a", "b", "c"};
  ui::Revision version{};
  std::size_t size() const override { return keys.size(); }
  ui::ItemKey keyAt(std::size_t i) const override { return keys.at(i); }
  ui::Revision revision() const override { return version; }
};

int main() {
  return test::run([] {
    auto source = std::make_shared<Source>();
    bool failCreate{};
    ui::ItemFactory factory{
        .create = [&](const ui::ItemKey &) -> std::unique_ptr<ui::Node> {
          if (failCreate)
            return {};
          return std::make_unique<ui::Button>(
              nullptr, ui::ButtonProps{},
              layout::BoxProps{.height = layout::SizeRule::fixed(20)});
        }};
    test::rejects([&] { ui::Repeat invalid{nullptr, factory}; },
                  "null source rejected");
    test::rejects([&] { ui::Repeat invalid{source, {}}; },
                  "empty factory rejected");
    ui::UIRoot root;
    auto repeat = std::make_unique<ui::Repeat>(source, factory);
    auto *r = repeat.get();
    root.setContent(std::move(repeat));
    root.flushLayout({100, 100});
    auto stable = r->realized("a")->handle();
    source->keys = {"a", "a"};
    test::rejects([&] { r->applyCollectionChanges(); },
                  "duplicate model keys rejected");
    test::require(stable && r->children().size() == 3,
                  "duplicate key failure preserves existing tree");
    source->keys = {"a", "d"};
    failCreate = true;
    test::rejects([&] { r->applyCollectionChanges(); },
                  "null item factory result rejected");
    test::require(stable && r->realized("b"),
                  "failed additions do not destroy retained children");
    failCreate = false;
    r->applyCollectionChanges();
    root.flushLayout({100, 100});
    test::require(stable && r->realized("d") && !r->realized("b"),
                  "retry converges to requested keys");
    source->keys.clear();
    r->applyCollectionChanges();
    root.flushLayout({100, 100});
    test::require(!stable && r->children().empty(),
                  "empty model removes nodes and expires handles");

    for (int i = 0; i < 20; ++i)
      source->keys.push_back(std::to_string(i));
    auto list = std::make_unique<ui::VirtualList>(
        source, factory, ui::VirtualListProps{.itemExtent = 20, .overscan = 0});
    auto *v = list.get();
    root.setContent(std::move(list));
    root.flushLayout({100, 40});
    auto focused = v->realized("0")->handle();
    root.requestFocus(focused.id());
    v->scrollToKey("15");
    root.flushLayout({100, 40});
    test::require(focused && v->realized("0"),
                  "offscreen focused item remains pinned");
    source->keys.erase(source->keys.begin());
    v->applyCollectionChanges();
    root.flushLayout({100, 40});
    test::require(!focused && !v->realized("0"),
                  "removed key is not kept alive by focus pinning");
    source->keys.erase(source->keys.begin());
    ++source->version;
    v->applyChanges({0,
                     1,
                     {{ui::CollectionOperation::Update, "1"},
                      {ui::CollectionOperation::Erase, "1"}}});
    root.flushLayout({100, 40});
    test::require(!v->realized("1"),
                  "update then erase does not refresh a removed key");
    {
      float modelHeight{10};
      auto model = std::make_shared<Source>();
      model->keys = {"one"};
      ui::ItemFactory views{[&](const auto &) -> std::unique_ptr<ui::Node> {
        return std::make_unique<ui::CustomView>(ui::CustomViewCallbacks{
            .measure = [&](ui::MeasureContext &,
                           const layout::SizeConstraints &) {
              return layout::MeasureResult{{20, modelHeight}};
            }});
      }};
      auto repeat = std::make_unique<ui::Repeat>(model, views);
      auto *probe = repeat.get();
      root.setContent(std::move(repeat));
      root.flushLayout({100, 100});
      test::require(probe->realized("one")->bounds().h() == 10,
                    "initial model-backed child measurement");
      modelHeight = 30;
      probe->applyCollectionChanges();
      root.flushLayout({100, 100});
      test::require(probe->realized("one")->bounds().h() == 30,
                    "full refresh invalidates retained child measurements");
      root.setContent({});
    }
  });
}
