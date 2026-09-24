#include <memory>
#include <string>

#include <support/AssetRegistry.hpp>
#include <support/Test.hpp>

int main() {
  return playground::test::run([] {
    using namespace playground;
    AssetRegistry assets;
    int loads{};
    auto create = [&] {
      ++loads;
      return SurfaceHandle{SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_RGBA32),
                           SurfaceHandleDeleter{}};
    };
    auto a = assets.getText("a", create);
    auto alias = assets.getText("a", create);
    test::require(a == alias && loads == 1,
                  "identical cache key reuses handle without calling factory");
    auto b = assets.getText("b", create);
    auto c = assets.getText("c", create);
    std::weak_ptr<SDL_Surface> weakA = a, weakB = b, weakC = c;
    const auto one = static_cast<std::size_t>(a->pitch) * a->h;
    test::require(assets.estimatedSurfaceBytes() == 3 * one,
                  "byte estimate is pitch times height per cached surface");
    a.reset();
    alias.reset();
    b.reset();
    c.reset();
    a = assets.getText("a", create);
    a.reset();
    assets.trimSurfaceBytes(2 * one);
    test::require(weakB.expired() && !weakA.expired() && !weakC.expired(),
                  "request recency evicts oldest unpinned surface");
    a = assets.getText("a", create);
    assets.trimSurfaceBytes(0);
    test::require(!weakA.expired() && weakC.expired() &&
                      assets.estimatedSurfaceBytes() == one,
                  "live handle pins cache above budget");
    a.reset();
    assets.trimSurfaceBytes(0);
    test::require(weakA.expired(), "last external release permits eviction");
    test::rejects(
        [&] { assets.getText("failed", [] { return SurfaceHandle{}; }); },
        "null factory result rejected");
    test::require(assets.estimatedSurfaceBytes() == 0,
                  "failed creation inserts no cache entry");
    test::rejects<std::runtime_error>(
        [&] {
          assets.getText("failed", []() -> SurfaceHandle {
            throw std::runtime_error("load failed");
          });
        },
        "factory exception propagates");
    a = assets.getText("failed", create);
    assets.clear();
    test::require(a && a->w == 8 && assets.estimatedSurfaceBytes() == 0,
                  "clear drops registry owners, not client resources");
  });
}
