#include <app/TTFGuard.hpp>
#include <array>
#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/GPUText.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Test.hpp>
#include <type_traits>
#include <ui/content/Image.hpp>
#include <ui/content/Text.hpp>
#include <ui/content/Vector.hpp>

using namespace playground;

class PreparedImage final : public ui::PaintImage {
  math::Size2 _size;

public:
  explicit PreparedImage(math::Size2 size) : _size{size} {}
  math::Size2 pixelSize() const noexcept override { return _size; }
};

class TestPreparer final : public rendering::ImagePreparer {
public:
  enum class Result { Valid, Null, WrongSize, Throw } result{Result::Valid};
  int calls{};
  ui::PaintImageHandle prepare(ui::PaintImageHandle source) override {
    ++calls;
    switch (result) {
    case Result::Null:
      return {};
    case Result::WrongSize:
      return std::make_shared<PreparedImage>(math::Size2{999, 999});
    case Result::Throw:
      throw std::runtime_error("simulated upload failure");
    default:
      return std::make_shared<PreparedImage>(source->pixelSize());
    }
  }
};

class PreparedPainter final : public ui::PaintContext {
public:
  TestPreparer images;
  ui::PaintImageHandle retained;
  int depth{};
  rendering::ImagePreparer *imagePreparer() noexcept override {
    return &images;
  }
  void save() override { ++depth; }
  void restore() noexcept override { --depth; }
  void translate(math::Vec2f) override {}
  void clip(math::Rect) override {}
  void fill(math::Rect, math::ColorRGBA8) override {}
  void drawImage(const ui::PaintImageHandle &image, math::Rect, math::Rect,
                 ui::ImagePaint) override {
    test::require(dynamic_cast<const PreparedImage *>(image.get()),
                  "paint receives realization rather than CPU source");
    retained = image;
  }
};

int main() {
  return test::run([] {
    static_assert(!std::is_copy_constructible_v<sdl::GPUImage>);
    static_assert(!std::is_copy_constructible_v<sdl::GPUText>);
    rendering::RGBA8Image pixels{.size = {2, 1},
                                 .pixels = {1, 2, 3, 4, 5, 6, 7, 8}};
    pixels.validate();
    pixels.pixels.pop_back();
    test::rejects([&] { pixels.validate(); }, "byte count validated");
    test::rejects([] { rendering::RGBA8Image::byteSize({0, 1}); },
                  "empty upload rejected");
    test::rejects([] { rendering::RGBA8Image::byteSize({-1, 1}); },
                  "negative upload rejected");
    pixels.pixels.push_back(8);
    pixels.alpha = static_cast<rendering::AlphaMode>(100);
    test::rejects([&] { pixels.validate(); }, "unknown alpha rejected");
    test::rejects([&] { sdl::GPUImage image{{}, pixels}; },
                  "device ownership required");
    test::rejects([] { sdl::GPUTextEngine engine{{}}; },
                  "text engine needs device");
    test::rejects([] { sdl::GPUImagePreparer images{{}}; },
                  "preparer needs device");

    // Three RGBA pixels with four padding bytes per row, no GPU/window needed.
    std::array<Uint8, 32> bytes{255, 0, 0,  128, 0,  255, 0,  255, 0,  0,
                                255, 0, 99, 99,  99, 99,  10, 20,  30, 40};
    SurfaceHandle surface{
        SDL_CreateSurfaceFrom(3, 2, SDL_PIXELFORMAT_RGBA32, bytes.data(), 16),
        SurfaceHandleDeleter{}};
    test::require(bool(surface), "padded test surface created");
    const auto source = sdl::makeSurfaceImage(surface);
    const auto packed = sdl::packSurfaceRGBA8(
        *std::dynamic_pointer_cast<const sdl::SurfacePaintImage>(source));
    test::require(packed.pixels.size() == 24 && packed.pixels[3] == 128 &&
                      packed.pixels[12] == 10 && packed.pixels[15] == 40,
                  "RGBA packing preserves alpha and excludes row padding");
    test::require(
        sdl::packSurfaceRGBA8(sdl::SurfacePaintImage{surface, true}).alpha ==
            rendering::AlphaMode::Premultiplied,
        "alpha convention retained");
    test::require(rendering::prepareImage(source, nullptr) == source,
                  "CPU identity path");
    test::rejects([] { rendering::prepareImage({}, nullptr); },
                  "null source rejected");
    TestPreparer bad;
    for (auto mode :
         {TestPreparer::Result::Null, TestPreparer::Result::WrongSize}) {
      bad.result = mode;
      test::rejects<std::runtime_error>(
          [&] { rendering::prepareImage(source, &bad); },
          "invalid realization rejected");
    }

    TTFGuard ttf;
    AssetRegistry assets;
    auto font = assets.getFont({.path = std::string{PLAYGROUND_SOURCE_DIR} +
                                        "/assets/fonts/LBRITE.TTF",
                                .style = {.size = 20}});
    auto document = std::make_shared<const SVGDocument>(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20"><rect width="20" height="20" fill="red"/></svg>)");
    sdl::UISession session;
    PreparedPainter painter;
    for (int kind = 0; kind < 3; ++kind) {
      if (kind == 0)
        session.root().setContent(
            std::make_unique<ui::Image>(ui::ImageProps{.image = source}));
      if (kind == 1)
        session.root().setContent(std::make_unique<ui::Text>(
            assets, ui::TextProps{.value = "abc", .font = font}));
      if (kind == 2)
        session.root().setContent(std::make_unique<ui::Vector>(
            assets, ui::VectorProps{.source = document}));
      session.synchronize({.windowSize = {80, 40}, .drawableSize = {80, 40}},
                          {});
      painter.images.result = TestPreparer::Result::Valid;
      const int before = painter.images.calls;
      session.render(painter);
      test::require(painter.images.calls == before + 1 && painter.retained &&
                        painter.depth == 0,
                    "session wires preparation and keeps painting balanced");
      painter.images.result = TestPreparer::Result::Throw;
      test::rejects<std::runtime_error>([&] { session.render(painter); },
                                        "upload failure propagated");
      test::rejects<std::logic_error>(
          [&] { session.root().render(painter); },
          "failed preparation cannot paint stale realization");
      painter.images.result = TestPreparer::Result::Valid;
      session.render(painter);
      test::require(painter.depth == 0,
                    "retry succeeds without leaking painter state");
    }
    session.root().setContent({});
    test::require(bool(painter.retained),
                  "recorded draw can retain image beyond node destruction");
  });
}
