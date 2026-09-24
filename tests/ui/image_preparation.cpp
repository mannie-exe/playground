#include <array>
#include <type_traits>

#include <app/TTFGuard.hpp>
#include <platform/sdl/GPUResources.hpp>
#include <platform/sdl/GPUText.hpp>
#include <platform/sdl/SurfacePainter.hpp>
#include <platform/sdl/UISession.hpp>
#include <support/Test.hpp>
#include <ui/content/Image.hpp>
#include <ui/content/Text.hpp>
#include <ui/content/Vector.hpp>

using namespace playground;

class PreparedImage final : public rendering::PaintImage {
  math::Size2 _size;
  rendering::AlphaMode _alpha;
  rendering::ColorEncoding _encoding;

public:
  explicit PreparedImage(
      math::Size2 size,
      rendering::AlphaMode alpha = rendering::AlphaMode::Straight,
      rendering::ColorEncoding encoding = rendering::ColorEncoding::SRGB)
      : _size{size}, _alpha{alpha}, _encoding{encoding} {}
  math::Size2 pixelSize() const noexcept override { return _size; }
  rendering::AlphaMode alphaMode() const noexcept override { return _alpha; }
  rendering::ColorEncoding colorEncoding() const noexcept override {
    return _encoding;
  }
};

class TestPreparer final : public rendering::ImagePreparer {
public:
  enum class Result {
    Valid,
    Null,
    WrongSize,
    WrongAlpha,
    WrongEncoding,
    Throw
  } result{Result::Valid};
  int calls{};
  rendering::PaintImageHandle
  prepare(rendering::PaintImageHandle source) override {
    ++calls;
    switch (result) {
    case Result::Null:
      return {};
    case Result::WrongSize:
      return std::make_shared<PreparedImage>(math::Size2{999, 999});
    case Result::WrongAlpha:
      return std::make_shared<PreparedImage>(
          source->pixelSize(), rendering::AlphaMode::Premultiplied);
    case Result::WrongEncoding:
      return std::make_shared<PreparedImage>(source->pixelSize(),
                                             source->alphaMode(),
                                             rendering::ColorEncoding::Linear);
    case Result::Throw:
      throw std::runtime_error("simulated upload failure");
    default:
      return std::make_shared<PreparedImage>(
          source->pixelSize(), source->alphaMode(), source->colorEncoding());
    }
  }
};

class PreparedPainter final : public rendering::PaintContext {
public:
  TestPreparer images;
  rendering::PaintImageHandle retained;
  int depth{};
  rendering::ImagePreparer *imagePreparer() noexcept override {
    return &images;
  }
  void save() override { ++depth; }
  void restore() noexcept override { --depth; }
  void translate(math::Vec2f) override {}
  void clip(math::Rect) override {}
  void fill(math::Rect, math::ColorRGBA8) override {}
  void drawImage(const rendering::PaintImageHandle &image, math::Rect,
                 math::Rect, rendering::ImagePaint) override {
    test::require(dynamic_cast<const PreparedImage *>(image.get()),
                  "paint receives realization rather than CPU source");
    retained = image;
  }
};

class TestTextPreparer final : public rendering::TextImagePreparer {
public:
  rendering::PaintImageHandle result;
  rendering::ResourceDomainId domain{rendering::acquireResourceDomain()};
  int calls{};
  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return domain;
  }
  rendering::PaintImageHandle
  prepareText(const rendering::TextSource &) override {
    ++calls;
    return result;
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
    pixels.alpha = rendering::AlphaMode::Straight;
    pixels.encoding = static_cast<rendering::ColorEncoding>(100);
    test::rejects([&] { pixels.validate(); }, "unknown encoding rejected");
    pixels.encoding = rendering::ColorEncoding::SRGB;
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
         {TestPreparer::Result::Null, TestPreparer::Result::WrongSize,
          TestPreparer::Result::WrongAlpha,
          TestPreparer::Result::WrongEncoding}) {
      bad.result = mode;
      test::rejects<std::runtime_error>(
          [&] { rendering::prepareImage(source, &bad); },
          "invalid realization rejected");
    }
    bad.result = TestPreparer::Result::Valid;
    for (bool premultiplied : {false, true}) {
      for (auto encoding :
           {rendering::ColorEncoding::SRGB, rendering::ColorEncoding::Linear}) {
        const auto declared =
            sdl::makeSurfaceImage(surface, premultiplied, encoding);
        const auto realization = rendering::prepareImage(declared, &bad);
        const auto packedImage = sdl::packSurfaceRGBA8(
            *std::dynamic_pointer_cast<const sdl::SurfacePaintImage>(declared));
        test::require(
            realization->colorEncoding() == encoding &&
                realization->alphaMode() == declared->alphaMode() &&
                packedImage.encoding == encoding &&
                packedImage.alpha == declared->alphaMode(),
            "all pixel representations survive packing and preparation");
      }
    }
    {
      sdl::SurfacePainter software{*surface};
      const auto linearSource = sdl::makeSurfaceImage(
          surface, false, rendering::ColorEncoding::Linear);
      test::rejects(
          [&] {
            software.drawImage(linearSource, math::rect(0, 0, 1, 1),
                               math::rect(0, 0, 1, 1), {});
          },
          "legacy software painter cannot reinterpret linear image bytes");
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
    TestTextPreparer textService;
    session.root().setContent(std::make_unique<ui::Text>(
        assets, ui::TextProps{.value = "abc", .font = font}));
    session.root().flushLayout({80, 40});
    test::rejects<std::runtime_error>(
        [&] { session.root().prepare({.text = &textService}); },
        "separate text service rejects null output");
    textService.result = std::make_shared<PreparedImage>(math::Size2{0, 20});
    test::rejects<std::runtime_error>(
        [&] { session.root().prepare({.text = &textService}); },
        "separate text service rejects invalid dimensions");
    textService.result = std::make_shared<PreparedImage>(math::Size2{30, 20});
    session.root().prepare({.text = &textService});
    session.root().render(painter);
    const int before = textService.calls;
    session.root().prepare({.text = &textService});
    test::require(textService.calls == before,
                  "text service does not need image service inheritance");
    textService.domain = rendering::acquireResourceDomain();
    session.root().prepare({.text = &textService});
    test::require(textService.calls == before + 1,
                  "text domain invalidates realized cache");
  });
}
