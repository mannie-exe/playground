#include <array>
#include <cmath>

#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    sdl::SoftwareSceneRenderer renderer;
    scene::SceneRenderProps view{{}, {4, 4}, {255, 255, 255, 128}};
    view.exposure = .25f;
    const auto output = renderer.render(view, {});
    const auto &image = dynamic_cast<const sdl::SurfacePaintImage &>(*output);
    math::ColorRGBA8 pixel;
    test::require(SDL_ReadSurfacePixel(image.surface().get(), 1, 1, &pixel.r,
                                       &pixel.g, &pixel.b, &pixel.a),
                  "read software scene exposure");
    test::require(std::abs(int(pixel.r) - 137) <= 1 && pixel.r == pixel.g &&
                      pixel.r == pixel.b && pixel.a == 128,
                  "linear exposure precedes encoding and preserves alpha");
    view.toneMap = true;
    test::rejects([&] { renderer.render(view, {}); },
                  "software cannot silently ignore tone mapping");
    view.toneMap = false;
    auto mesh = scene::makeMesh(
        {{{{-.5f, -.5f, .5f}}, {{.5f, -.5f, .5f}}, {{0, .5f, .5f}}},
         {0, 1, 2}});
    scene::MaterialProps material;
    material.pbr.emplace();
    std::array draws{scene::MeshDraw{mesh, material, {}}};
    test::rejects([&] { renderer.render(view, draws); },
                  "software requires explicit PBR preview conversion");
    draws[0].material = scene::unlitPreview(material);
    test::require(bool(renderer.render(view, draws)),
                  "explicit unlit preview renders");
    view = {{}, {32, 32}, {0, 0, 0, 255}};
    draws[0].material = {};
    auto &binding = draws[0].material.colorTexture;
    binding.texture = rendering::makeTexture(
        {{2, 1}, {{1, 0, 0, 0}, {0, 1, 0, 1}}}, rendering::TextureRole::Color);
    binding.transform = {{.5f, .5f}, {0, 0}};
    const auto sample = [&] {
      const auto result = renderer.render(view, draws);
      const auto &surface =
          dynamic_cast<const sdl::SurfacePaintImage &>(*result);
      math::ColorRGBA8 c;
      test::require(SDL_ReadSurfacePixel(surface.surface().get(), 16, 16, &c.r,
                                         &c.g, &c.b, &c.a),
                    "read software material alpha regression");
      return c;
    };
    auto opaque = sample();
    test::require(std::abs(int(opaque.r) - 188) <= 1 && opaque.r == opaque.g,
                  "software opaque filtering includes invisible texel RGB");
    draws[0].material.alpha = scene::MaterialProps::Alpha::Blend;
    auto blended = sample();
    test::require(blended.r == 0 && std::abs(int(blended.g) - 188) <= 1,
                  "software blend keeps coverage-weighted filtering");
    binding = {};
    SurfaceHandle transparentRed{
        SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
        SurfaceHandleDeleter{}};
    test::require(
        bool(transparentRed) &&
            SDL_WriteSurfacePixel(transparentRed.get(), 0, 0, 255, 0, 0, 0),
        "create software straight transparent image");
    draws[0].material.baseColorImage = sdl::makeSurfaceImage(transparentRed);
    for (const auto filter :
         {rendering::Sampling::Nearest, rendering::Sampling::Linear}) {
      draws[0].material.sampling = filter;
      draws[0].material.alpha = scene::MaterialProps::Alpha::Opaque;
      test::require(sample().r == 255,
                    "software opaque PaintImage preserves hidden RGB");
      draws[0].material.alpha = scene::MaterialProps::Alpha::Blend;
      test::require(sample().r == 0,
                    "software blended PaintImage preserves coverage");
    }
  });
}
