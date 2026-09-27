#include <memory>
#include <string>

#include <app/TTFGuard.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Text.hpp>
#include <ui/controls/Composite.hpp>

using namespace playground;

class Paint final : public rendering::PaintContext {
public:
  math::Size2 drawn;
  math::Size2 pixels;

  void save() override {}

  void restore() noexcept override {}

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect, math::ColorRGBA8) override {}

  void drawPath(const math::Path2D &, const rendering::PathPaint &) override {}

  void drawImage(const rendering::PaintImageHandle &image, math::Rect,
                 math::Rect destination, rendering::ImagePaint) override {
    drawn = destination.size;
    pixels = image->pixelSize();
  }
};

int main() {
  return test::run([] {
    TTFGuard ttf;
    AssetRegistry assets;
    const auto font =
        assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                "/assets/fonts/LBRITE.TTF",
                        .style = {.size = 18}});
    for (const auto *value :
         {"Push button", "Disabled button", "Switch", "Show/hide tooltip",
          "Open modal dialog", "A longer label that should genuinely wrap"}) {
      for (float width : {90.f, 140.f, 220.f, 400.f})
        for (float scale : {1.f, 1.25f, 1.5f, 2.f})
          for (int kind = 0; kind < 4; ++kind) {
            auto text = std::make_unique<ui::Text>(
                assets,
                ui::TextProps{.value = value,
                              .font = font,
                              .wrap = ui::TextWrap::AvailableInlineSize});
            auto *label = text.get();
            std::unique_ptr<ui::Node> button;
            if (kind == 0)
              button = std::make_unique<ui::Button>(
                  std::move(text), ui::ButtonProps{},
                  layout::BoxProps{.padding = math::Insets::all(10)});
            else if (kind == 1)
              button = std::make_unique<ui::Checkbox>(std::move(text));
            else if (kind == 2)
              button = std::make_unique<ui::Switch>(std::move(text));
            else
              button = std::make_unique<ui::Select>(
                  std::move(text), std::vector<ui::ChoiceItem>{});
            ui::MeasureContext context;
            context.pixelScale = {scale, scale};
            const auto measured = button->measure(context, {{0, width}, {}});
            ui::UIRoot root;
            root.setContent(std::move(button));
            root.flushLayout(ui::LayoutEnvironment{
                .viewport = measured.size, .pixelScale = {scale, scale}});
            root.prepare({.pixelScale = {scale, scale}});
            Paint paint;
            root.render(paint);
            test::require(paint.pixels.height <=
                              label->bounds().h() * scale + 1,
                          "raster agrees with measured height before "
                          "destination scaling");
            test::require(paint.drawn.height <= label->bounds().h() + 1.f,
                          std::string{"painted text exceeds measured label: "} +
                              value + " at " + std::to_string(width) +
                              " scale " + std::to_string(scale));
            const auto extent = paint.drawn;
            root.prepare({.pixelScale = {scale * 1.3f, scale * 1.3f}});
            root.render(paint);
            test::require(
                paint.drawn == extent,
                "render-only scaling preserves logical text geometry");
          }
    }
  });
}
