#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Boundaries.hpp>

#include <limits>
#include <memory>

namespace {
using namespace playground;

class Image final : public ui::PaintImage {
  math::Size2 _size;

public:
  explicit Image(math::Size2 size) : _size{size} {}
  math::Size2 pixelSize() const noexcept override { return _size; }
};

class Painter final : public ui::PaintContext {
public:
  ui::PaintImageHandle result;
  int captures{};
  int draws{};
  int depth{};

  void save() override { ++depth; }
  void restore() noexcept override { --depth; }
  void translate(math::Vec2f) override {}
  void clip(math::Rect) override {}
  void fill(math::Rect, math::ColorRGBA8) override {}
  ui::PaintImageHandle
  capture(math::Rect, math::Vec2f,
          const std::function<void(ui::PaintContext &)> &) override {
    ++captures;
    return result;
  }
  void drawImage(const ui::PaintImageHandle &, math::Rect, math::Rect,
                 ui::ImagePaint) override {
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
  });
}
