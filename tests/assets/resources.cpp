#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

#include <app/SDLGuard.hpp>
#include <app/TTFGuard.hpp>
#include <platform/sdl/AssetResources.hpp>
#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Text.hpp>

using namespace playground;

int main() {
  return test::run([] {
    SDLGuard sdl{0};
    TTFGuard ttf;
    auto catalog = std::make_shared<assets::AssetCatalog>(
        std::filesystem::path{PLAYGROUND_SOURCE_DIR} / "assets");
    const assets::AssetId<assets::FontAsset> fontId{"font"};
    const assets::AssetId<assets::ImageAsset> imageId{"image"},
        brokenId{"broken"};
    const assets::AssetId<assets::VectorAsset> vectorId{"vector"},
        ownedId{"owned"};
    catalog->add(fontId, assets::FontAsset{{"fonts/LBRITE.TTF"}});
    catalog->add(imageId, assets::ImageAsset{assets::FileSource{
                              "minesweeper/images/bomb.png"}});
    catalog->add(brokenId,
                 assets::ImageAsset{assets::ByteSource{{std::byte{0}}}});
    catalog->add(vectorId, assets::VectorAsset{assets::FileSource{
                               "minesweeper/images/flag.svg"}});
    const auto xml =
        std::string{"<svg xmlns='http://www.w3.org/2000/svg' width='8' "
                    "height='8'><rect width='8' height='8'/></svg>"};
    const auto bytes = std::as_bytes(std::span{xml.data(), xml.size()});
    catalog->add(ownedId, assets::VectorAsset{assets::ByteSource{
                              {bytes.begin(), bytes.end()}}});
    catalog->freeze();
    auto ledger = std::make_shared<rendering::ResourceLedger>();
    const auto globalBefore =
        rendering::defaultResourceLedger()->snapshot().memory[0].bytes;
    AssetRegistry cache{ledger};
    sdl::AssetResources resources{catalog, cache};
    auto font = resources.font(fontId, {.style = {.size = 18}});
    test::require(font == resources.font(fontId, {.style = {.size = 18}}),
                  "font variant shares handle");
    test::require(font != resources.font(fontId, {.style = {.size = 24}}),
                  "font size is part of variant");
    test::require(font->cloneWith({.size = 20.0f}).props().cacheIdentity ==
                      font->props().cacheIdentity,
                  "font clone retains catalog source identity");
    auto image = resources.image(imageId);
    test::require(image == resources.image(imageId),
                  "image identity shares surface");
    test::require(
        ledger->snapshot().memory[0].bytes > 0 &&
            rendering::defaultResourceLedger()->snapshot().memory[0].bytes ==
                globalBefore &&
            font->cloneWith({.size = 20}).resources() == ledger,
        "decoded surfaces and font variants belong to the injected cache "
        "domain");
    auto vector = resources.vector(vectorId);
    test::require(vector == resources.vector(vectorId),
                  "vector identity shares document");
    auto owned = resources.vector(ownedId);
    test::require(cache.getVector(VectorSource{owned}, {}, {16, 16})->w == 16,
                  "owned vector source rasterizes");
    test::rejects<std::runtime_error>([&] { resources.image(brokenId); },
                                      "bad decoder input is failure");
    test::require(resources.image(imageId) == image,
                  "failed acquisition preserves existing cache");
    sdl::AssetResources limited{catalog, cache, {.maxDecodedImageBytes = 1}};
    test::rejects<std::length_error>(
        [&] { limited.image(imageId); },
        "cached images still obey acquisition policy");
    test::rejects(
        [&] {
          sdl::AssetResources invalid{catalog, cache, {.maxSVGBytes = 0}};
        },
        "invalid resource policy rejected");
    std::atomic<bool> rejected{};
    std::jthread worker{[&] {
      try {
        resources.image(imageId);
      } catch (const std::logic_error &) {
        rejected = true;
      }
    }};
    worker.join();
    test::require(rejected,
                  "native cache acquisition rejects worker-thread mutation");

    // Reconstruct a generic resource-backed UI from app-owned state, not old
    // nodes.
    struct Model {
      std::string value{"persistent"};
    } model;
    ui::UIRoot root;
    const auto build = [&] {
      return std::make_unique<ui::Text>(
          cache, ui::TextProps{.value = model.value, .font = font});
    };
    auto first = build();
    auto *firstPointer = first.get();
    root.setContent(std::move(first));
    auto previous = firstPointer->handle<ui::Text>();
    root.flushLayout({200, 50});
    model.value = "edited";
    auto candidate = build();
    auto *pointer = candidate.get();
    root.setContent(std::move(candidate));
    root.flushLayout({200, 50});
    test::require(
        !previous.get() && pointer->props().value == "edited" &&
            model.value == "edited",
        "reconstruction creates fresh nodes without resetting the model");
    resources.trimUnused();
    cache.trim(0, 0, 0);
    cache.trimSurfaceBytes(0);
    test::require(
        resources.font(fontId, {.style = {.size = 18}}) == font &&
            resources.vector(vectorId) == vector,
        "live resource bundle survives trimming and view replacement");
  });
}
