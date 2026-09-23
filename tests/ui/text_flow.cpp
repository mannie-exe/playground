#include <app/TTFGuard.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <support/Test.hpp>
#include <support/VerticalOrientation.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Text.hpp>

int main() {
  return playground::test::run([] {
    using namespace playground;
    using namespace playground::ui;
    test::require(graphemeBoundaries("e\xCC\x81x") ==
                      std::vector<std::size_t>{0, 3, 4},
                  "combining sequence stays intact");
    const std::string family = "\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    test::require(graphemeBoundaries(family).size() == 2,
                  "emoji ZWJ sequence stays intact");
    test::rejects([] { graphemeBoundaries("\xFF"); }, "invalid UTF-8 rejected");
    for (auto mode :
         {TextTruncation::EllipsisStart, TextTruncation::EllipsisMiddle,
          TextTruncation::EllipsisEnd}) {
      const auto result =
          truncateText("abcdef", {.truncation = mode, .ellipsis = "."},
                       [](std::string_view text) { return text.size() <= 4; });
      test::require(result.size() == 4 && result.find('.') != std::string::npos,
                    "all ellipsis locations fit");
    }
    TTFGuard ttf;
    AssetRegistry assets;
    auto font = assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                        "/assets/fonts/LBRITE.TTF",
                                .style = {.size = 20}});
    UIRoot root;
    auto text = std::make_unique<Text>(
        assets, TextProps{.value = "alpha beta gamma delta",
                          .font = font,
                          .flow = {.truncation = TextTruncation::EllipsisEnd}});
    auto *node = text.get();
    root.setContent(std::move(text));
    root.flushLayout({70, 30});
    test::require(node->isTruncated() &&
                      node->props().value == "alpha beta gamma delta",
                  "derived truncated text preserves original value");
    root.prepare();
    const auto saved = node->displayedValue();
    test::require(!saved.empty(), "ellipsis fits nonempty target");
    root.flushLayout({1, 30});
    root.prepare();
    test::require(node->displayedValue().empty(),
                  "even ellipsis too wide yields no glyphs");
    auto props = node->props();
    props.wrap = TextWrap::AvailableInlineSize;
    props.flow.maximumLines = 1;
    node->setProps(props);
    root.flushLayout({70, 200});
    root.prepare();
    test::require(node->isTruncated(), "line cap truncates wrapped text");
    test::require(verticalOrientation(U'A') == VerticalOrientation::R &&
                      verticalOrientation(U'漢') == VerticalOrientation::U,
                  "Unicode vertical orientation lookup");
    for (auto mode : {WritingMode::VerticalLr, WritingMode::VerticalRl}) {
      for (auto orientation : {TextOrientation::Mixed, TextOrientation::Upright,
                               TextOrientation::Sideways}) {
        auto columns =
            sdl::layoutTextColumns(assets, font, "ABCD\nEF", 45.0f, mode,
                                   orientation, layout::Align::Center);
        test::require(columns.count >= 2 && !columns.runs.empty(),
                      "vertical wrapping produces columns");
        const auto firstX = columns.runs.front().bounds.x();
        const auto lastX = columns.runs.back().bounds.x();
        test::require(mode == WritingMode::VerticalLr ? firstX < lastX
                                                      : firstX > lastX,
                      "column progression follows writing mode");
        auto verticalProps = node->props();
        verticalProps.value = "ABCD\nEF";
        verticalProps.flow = {.writingMode = mode, .orientation = orientation};
        node->setProps(verticalProps);
        root.flushLayout({120, 45});
        root.prepare();
        SurfaceHandle surface{
            requireSDL(SDL_CreateSurface(120, 45, SDL_PIXELFORMAT_RGBA32),
                       "text test"),
            SurfaceHandleDeleter{}};
        SDL_FillSurfaceRect(surface.get(), nullptr, 0);
        sdl::SurfacePainter painter{*surface};
        root.render(painter);
        bool painted{};
        for (int y = 0; y < surface->h; ++y)
          for (int x = 0; x < surface->w; ++x) {
            Uint8 r{}, g{}, b{}, a{};
            SDL_ReadSurfacePixel(surface.get(), x, y, &r, &g, &b, &a);
            painted |= a != 0;
          }
        test::require(painted, "vertical text emits pixels");
      }
    }
    test::require(font->getDirection() == TTF_DIRECTION_LTR,
                  "vertical layout leaves shared original font untouched");
    auto invalid = node->props();
    invalid.method = TextMethod::LCD;
    test::rejects([&] { node->setProps(invalid); },
                  "vertical LCD reports unsupported subpixel orientation");
    test::rejects([&] { sdl::sidewaysCluster("א", TextOrientation::Mixed); },
                  "vertical bidi explicitly unsupported");
    test::rejects([&] { sdl::sidewaysCluster("（", TextOrientation::Mixed); },
                  "unverifiable Tr fallback reported");
  });
}
