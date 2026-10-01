#include <limits>
#include <memory>

#include <rendering/ResourceLedger.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>

namespace {
using namespace playground;

class Image final : public rendering::PaintImage {
  math::Size2 _size;

public:
  explicit Image(math::Size2 size) : _size{size} {}

  math::Size2 pixelSize() const noexcept override { return _size; }
};

class Content final : public ui::Node {
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &) override {
    return {.size = {10, 10}};
  }

  void paint(ui::PaintContext &context) const override {
    context.fill(bounds(), {255, 0, 0, 255});
  }

public:
  Content()
      : Node{layout::BoxProps{.width = layout::SizeRule::fixed(10),
                              .height = layout::SizeRule::fixed(10)}} {}
};

class Painter final : public rendering::PaintContext {
public:
  rendering::PaintImageHandle result;
  int captures{};
  int draws{};
  int depth{}, fills{}, pressure{};

  void save() override { ++depth; }

  void restore() noexcept override { --depth; }

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect, math::ColorRGBA8) override { ++fills; }

  rendering::PaintImageHandle
  capture(math::Rect, math::Vec2f,
          const std::function<void(rendering::PaintContext &)> &draw) override {
    ++captures;
    if (pressure) {
      if (pressure == 2)
        draw(*this);
      throw rendering::ResourcePressure("Optional capture", 400, 400, 400);
    }
    return result;
  }

  void drawImage(const rendering::PaintImageHandle &, math::Rect, math::Rect,
                 rendering::ImagePaint) override {
    ++draws;
  }
};
} // namespace

int main() {
  return test::run([] {
    ui::UIRoot root;
    root.setContent(std::make_unique<ui::Layer>(
        nullptr,
        ui::LayerProps{.cachePolicy = ui::LayerCachePolicy::WhenUnchanged},
        layout::BoxProps{.width = layout::SizeRule::fixed(10),
                         .height = layout::SizeRule::fixed(10)}));
    root.flushLayout({10, 10});
    root.prepare();
    Painter painter;
    const auto reject = [&] {
      test::rejects<std::runtime_error>([&] { root.render(painter); },
                                        "malformed capture must be rejected");
      test::require(painter.depth == 0 && painter.draws == 0 &&
                        root.services().rasterBudget->used == 0,
                    "failed capture restores painter and releases reservation");
    };
    reject();
    for (auto size :
         {math::Size2{0, 10}, math::Size2{-1, 10},
          math::Size2{std::numeric_limits<float>::quiet_NaN(), 10},
          math::Size2{10, std::numeric_limits<float>::infinity()}}) {
      painter.result = std::make_shared<Image>(size);
      reject();
    }
    painter.result = std::make_shared<Image>(math::Size2{10, 10});
    root.render(painter);
    root.render(painter);
    test::require(painter.captures == 6 && painter.draws == 2 &&
                      root.services().rasterBudget->used == 400,
                  "successful retry commits and reuses one cache reservation");
    root.setContent({});
    test::require(root.services().rasterBudget->used == 0,
                  "detachment releases successful capture");
    root.setContent(std::make_unique<ui::Layer>(
        std::make_unique<Content>(),
        ui::LayerProps{.cachePolicy = ui::LayerCachePolicy::WhenUnchanged},
        layout::BoxProps{.width = layout::SizeRule::fixed(10),
                         .height = layout::SizeRule::fixed(10)}));
    root.flushLayout({10, 10});
    root.prepare();
    painter.pressure = 1;
    root.render(painter);
    test::require(
        painter.fills == 1 && root.services().rasterBudget->used == 0,
        "optional cache refusal before recording paints uncached once");
    painter.pressure = 2;
    painter.fills = 0;
    test::rejects<rendering::ResourcePressure>(
        [&] { root.render(painter); },
        "pressure after content recording propagates without replay");
    test::require(painter.fills == 1 && painter.depth == 0 &&
                      root.services().rasterBudget->used == 0,
                  "partially recorded content is not painted twice");
  });
}
