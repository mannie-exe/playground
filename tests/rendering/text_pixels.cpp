#include <algorithm>
#include <cmath>
#include <string>

#include <SDL3_ttf/SDL_textengine.h>

#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <support/AssetRegistry.hpp>
#include <support/Font.hpp>
#include <support/SDLResource.hpp>
#include <support/Test.hpp>

using namespace playground;

static void verifyLineSpacing(const std::string &path) {
  FontProps props{.path = path, .style = {.size = 32}};
  Font font{props};
  FontResource native{TTF_OpenFont(path.c_str(), 32)};
  test::require(
      native && !font.getLineSpace() &&
          font.getLineSkip() == TTF_GetFontLineSkip(native.get()),
      "default line spacing uses natural font metrics, not one pixel");
  const int initialSkip = font.getLineSkip();

  SDLResource<TTF_Text, TTF_DestroyText> text{
      TTF_CreateText(nullptr, font.get(), "A\nA", 3)};
  test::require(text && TTF_UpdateText(text.get()),
                "layout default multiline text");
  const TTF_CopyOperation *first{}, *second{};
  for (int i = 0; i < text->internal->num_ops; ++i) {
    const auto &operation = text->internal->ops[i];
    if (operation.cmd != TTF_DRAW_COMMAND_COPY)
      continue;
    if (!first)
      first = &operation.copy;
    else
      second = &operation.copy;
  }
  test::require(
      first && second && second->dst.y - first->dst.y == initialSkip &&
          first->dst.y + first->dst.h <= second->dst.y,
      "default multiline glyphs use natural baseline advance without overlap");
  text.reset();

  font.setLineSpace(initialSkip + 7);
  font.applyProps({});
  test::require(font.getLineSpace() == initialSkip + 7 &&
                    font.getLineSkip() == initialSkip + 7,
                "empty patch keeps an explicit line advance");
  font.setSize(48);
  font.setStyleFlags(TTF_STYLE_BOLD);
  font.setOutline(1);
  test::require(font.getLineSkip() == initialSkip + 7,
                "font metric changes retain authored pixel line advance");

  FontPatch reset;
  reset.lineSpace.emplace(std::nullopt);
  font.applyProps(reset);
  Font natural{font.getProps()};
  test::require(
      !font.getLineSpace() && font.getLineSkip() == natural.getLineSkip(),
      "engaged empty patch restores freshly configured natural line skip");
  font.applyProps({.lineSpace = std::optional<int>{75}});
  test::require(font.getLineSpace() == 75 && font.getLineSkip() == 75,
                "engaged pixel patch sets actual line skip");
  font.setLineSpace(std::nullopt);
  font.setSize(64);
  Font enlarged{font.getProps()};
  test::require(!font.getLineSpace() &&
                    font.getLineSkip() == enlarged.getLineSkip() &&
                    font.getLineSkip() > initialSkip,
                "automatic line advance recomputes after font size changes");
  test::rejects([&] { font.setLineSpace(0); }, "reject zero line advance");
  test::require(!font.getLineSpace() &&
                    font.getLineSkip() == enlarged.getLineSkip(),
                "invalid line advance preserves prior state");

  AssetRegistry assets;
  const auto automatic = assets.getFont(props);
  props.layout.lineSpace = automatic->getLineSkip();
  const auto explicitSkip = assets.getFont(props);
  test::require(
      automatic != explicitSkip && !automatic->getLineSpace() &&
          explicitSkip->getLineSpace().has_value(),
      "font cache distinguishes automatic from coincident explicit advance");
  props.layout.lineSpace = 0;
  test::rejects([&] { (void)assets.getFont(props); },
                "font construction rejects nonpositive explicit line advance");
}

static void verifyOperations(Font &font, const std::string &value, int wrap,
                             bool expectColor = false) {
  constexpr SDL_Color foreground{60, 160, 230, 192};
  SDLResource<TTF_Text, TTF_DestroyText> text{
      TTF_CreateText(nullptr, font.get(), value.data(), value.size())};
  test::require(text && TTF_SetTextWrapWidth(text.get(), wrap) &&
                    TTF_UpdateText(text.get()),
                "create layout-only text operations");
  SDLResource<SDL_Surface, SDL_DestroySurface> raster{
      TTF_RenderText_Blended_Wrapped(font.get(), value.data(), value.size(),
                                     foreground, wrap)};
  test::require(bool(raster), "render reference straight-alpha text surface");
  bool sawColor{}, sawMono{}, sawPartial{};
  for (int i = 0; i < text->internal->num_ops; ++i) {
    const auto &operation = text->internal->ops[i];
    if (operation.cmd != TTF_DRAW_COMMAND_COPY)
      continue;
    const auto &copy = operation.copy;
    TTF_ImageType type{};
    SDLResource<SDL_Surface, SDL_DestroySurface> glyph{
        TTF_GetGlyphImageForIndex(copy.glyph_font, copy.glyph_index, &type)};
    test::require(bool(glyph), "get exact glyph used by shaped operation");
    sawColor |= type == TTF_IMAGE_COLOR;
    sawMono |= type == TTF_IMAGE_ALPHA;
    test::require(copy.src.w == copy.dst.w && copy.src.h == copy.dst.h &&
                      copy.src.x >= 0 && copy.src.y >= 0 &&
                      copy.src.x + copy.src.w <= glyph->w &&
                      copy.src.y + copy.src.h <= glyph->h,
                  "glyph operations crop actual pixels without stretching");
    for (int y = 0; y < copy.src.h; ++y)
      for (int x = 0; x < copy.src.w; ++x) {
        SDL_Color source{}, actual{};
        test::require(SDL_ReadSurfacePixel(glyph.get(), copy.src.x + x,
                                           copy.src.y + y, &source.r, &source.g,
                                           &source.b, &source.a) &&
                          SDL_ReadSurfacePixel(raster.get(), copy.dst.x + x,
                                               copy.dst.y + y, &actual.r,
                                               &actual.g, &actual.b, &actual.a),
                      "glyph and string pixel bounds agree");
        if (!source.a)
          continue;
        sawPartial |= source.a < 255;
        const auto straight = [&](Uint8 component) {
          return std::min(255,
                          (int(component) * 255 + source.a / 2) / source.a);
        };
        const int r =
            type == TTF_IMAGE_COLOR ? straight(source.r) : foreground.r;
        const int g =
            type == TTF_IMAGE_COLOR ? straight(source.g) : foreground.g;
        const int b =
            type == TTF_IMAGE_COLOR ? straight(source.b) : foreground.b;
        const int a = int(source.a) * foreground.a / 255;
        test::require(std::abs(int(actual.r) - r) <= 1 &&
                          std::abs(int(actual.g) - g) <= 1 &&
                          std::abs(int(actual.b) - b) <= 1 &&
                          std::abs(int(actual.a) - a) <= 1,
                      "CPU string and glyph operations share bearings and "
                      "straight-alpha semantics");
      }
  }
  test::require(
      sawMono && sawPartial && (!expectColor || sawColor),
      "fixture exercises monochrome, partial alpha and requested color runs");
}

int main() {
  return test::run([] {
    SDLGuard sdl{0};
    TTFGuard ttf;
    verifyLineSpacing(std::string{PLAYGROUND_SOURCE_DIR} +
                      "/assets/fonts/LBRITE.TTF");
    Font font{FontProps{.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                "/assets/fonts/LBRITE.TTF",
                        .style = {.size = 32},
                        .layout = {.lineSpace = 64}}};
    verifyOperations(font, "A   K", 0);
    font.setStyleFlags(TTF_STYLE_BOLD | TTF_STYLE_ITALIC);
    font.setOutline(2);
    verifyOperations(font, "A   K", 0);
    font.setStyleFlags(TTF_STYLE_NORMAL);
    font.setOutline(0);
#ifdef PLAYGROUND_COLOR_FONT
    Font emoji{FontProps{.path = PLAYGROUND_COLOR_FONT,
                         .style = {.size = 32},
                         .layout = {.lineSpace = 64}}};
    test::require(TTF_AddFallbackFont(font.get(), emoji.get()),
                  "register fixture color fallback");
    struct FallbackScope {
      TTF_Font *font;
      TTF_Font *fallback;
      ~FallbackScope() { TTF_RemoveFallbackFont(font, fallback); }
    } fallback{font.get(), emoji.get()};
    verifyOperations(font, "A   \xE3\x8A\x97   K", 0, true);
    verifyOperations(font, "A   \xE3\x8A\x97   K", 100, true);
#endif
  });
}
