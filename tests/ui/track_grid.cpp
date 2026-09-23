#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/VirtualTrackGrid.hpp>
#include <ui/content/Rectangle.hpp>

using namespace playground;

struct Source : ui::GridSource {
  std::vector<ui::ItemKey> keys{"header", "span", "bottom"};
  std::vector<ui::GridItemPlacement> cells{{0, 0, 1, 2}, {1, 0, 3, 1}, {4, 1}};
  ui::Revision version{};
  std::size_t size() const override { return keys.size(); }
  ui::ItemKey keyAt(std::size_t i) const override { return keys.at(i); }
  ui::Revision revision() const override { return version; }
  ui::GridItemPlacement placementAt(std::size_t i) const override {
    return cells.at(i);
  }
};

int main() {
  return test::run([] {
    auto source = std::make_shared<Source>();
    ui::ItemFactory factory{[](const auto &) -> std::unique_ptr<ui::Node> {
      auto node =
          std::make_unique<ui::Rectangle>(ui::RectangleProps{{255, 0, 0, 255}});
      node->setHitTestPolicy(ui::HitTestPolicy::Self);
      return node;
    }};
    ui::VirtualTrackGridProps props{.columns = {{40}, {50}},
                                    .rows = {{20}, {30}, {40}, {50}, {60}},
                                    .frozen = {.rowsStart = 1},
                                    .overscan = 0};
    ui::UIRoot root;
    auto grid = std::make_unique<ui::VirtualTrackGrid>(source, factory, props);
    auto *node = grid.get();
    root.setContent(std::move(grid));
    root.flushLayout({90, 90});
    const auto header = node->item("header")->handle();
    node->setOffset({0, 80});
    root.flushLayout({90, 90});
    test::require(node->item("span") != nullptr,
                  "span origin above viewport remains realized");
    test::require(node->item("header")->worldTransform().mapPoint({0, 0}).y ==
                      0,
                  "frozen leading row remains stationary");
    auto hit = root.hitTest({5, 5});
    test::require(hit && hit->target.get() == header.get(),
                  "body does not steal frozen-pane hits");
    source->keys.push_back("added");
    source->cells.push_back({4, 0});
    ++source->version;
    node->applyChanges({0, 1, {{ui::CollectionOperation::Insert, "added", 3}}});
    root.flushLayout({90, 90});
    test::require(header && node->item("added"),
                  "delta insertion preserves existing nodes");
    test::rejects([&] { node->applyChanges({0, 1, {}}); },
                  "stale delta rejected");
    auto invalid = props;
    invalid.frozen.rowsStart = 2;
    test::rejects([&] { node->setProps(invalid); },
                  "span crossing frozen boundary rejected");
    test::require(node->props() == props,
                  "failed props retain previous configuration");
    root.flushLayout({90, 90});
    test::rejects([&] { root.flushLayout({90, 10}); },
                  "frozen panes must fit viewport");
    root.flushLayout({90, 90});
    const float before =
        node->item("span")->worldTransform().mapPoint({0, 0}).y;
    source->cells[1] = {2, 0, 2, 1};
    ++source->version;
    node->applyChanges({1, 2, {{ui::CollectionOperation::Update, "span"}}});
    root.flushLayout({90, 90});
    test::require(node->item("span")->worldTransform().mapPoint({0, 0}).y ==
                      before,
                  "model relocation retains visible key anchor");
    auto fixed = std::make_shared<Source>();
    fixed->keys = {"item"};
    fixed->cells = {{0, 0}};
    ui::ItemFactory sized{[](const auto &) -> std::unique_ptr<ui::Node> {
      return std::make_unique<ui::Rectangle>(
          ui::RectangleProps{},
          layout::BoxProps{.width = layout::SizeRule::fixed(60),
                           .height = layout::SizeRule::fixed(30)});
    }};
    auto estimated = std::make_unique<ui::VirtualTrackGrid>(
        fixed, sized,
        ui::VirtualTrackGridProps{
            .columns = {{20, true}}, .rows = {{10, true}}, .overscan = 0});
    auto *estimatedNode = estimated.get();
    root.setContent(std::move(estimated));
    root.flushLayout({90, 90});
    test::require(estimatedNode->contentExtent() == math::Size2{60, 30},
                  "estimated tracks grow from intrinsic content");
    auto frozenProps = estimatedNode->props();
    frozenProps.frozen = {.rowsEnd = 1, .columnsEnd = 1};
    estimatedNode->setProps(frozenProps);
    root.flushLayout({90, 90});
    test::require(estimatedNode->item("item")->worldTransform().mapPoint(
                      {0, 0}) == math::Point2{30, 60},
                  "trailing frozen tracks stay at viewport edges");
    fixed->keys.clear();
    fixed->cells.clear();
    ++fixed->version;
    estimatedNode->applyChanges(
        {0, 1, {{ui::CollectionOperation::Erase, "item"}}});
    fixed->keys = {"replacement"};
    fixed->cells = {{0, 0}};
    ++fixed->version;
    estimatedNode->applyChanges(
        {1, 2, {{ui::CollectionOperation::Insert, "replacement", 0}}});
    root.flushLayout({90, 90});
    test::require(estimatedNode->item("replacement") &&
                      !estimatedNode->item("item"),
                  "consecutive edits work without an intervening frame");
    fixed->keys.clear();
    fixed->cells.clear();
    ++fixed->version;
    estimatedNode->applyChanges(
        {2, 3, {{ui::CollectionOperation::Erase, "replacement"}}});
    test::rejects([&] { root.flushLayout({10, 10}); },
                  "empty grid still validates frozen pane extents");
    {
      auto refining = std::make_shared<Source>();
      refining->keys = {"first", "second"};
      refining->cells = {{0, 0}, {1, 0}};
      auto grid = std::make_unique<ui::VirtualTrackGrid>(
          refining, sized,
          ui::VirtualTrackGridProps{.columns = {{60}},
                                    .rows = {{10, true}, {10, true}},
                                    .overscan = 0});
      auto *probe = grid.get();
      root.setContent(std::move(grid));
      root.flushLayout({60, 30});
      root.flushLayout({60, 30});
      test::require(probe->item("first") && !probe->item("second"),
                    "refinement repeats visibility selection instead of "
                    "caching the estimated range");
    }
  });
}
