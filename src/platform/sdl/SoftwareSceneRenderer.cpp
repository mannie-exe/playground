#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <math/ColorSpace.hpp>
#include <platform/sdl/SoftwareSceneRenderer.hpp>
#include <platform/sdl/SurfacePaintImage.hpp>
#include <rendering/ImageData.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {
namespace {
struct Vertex {
  math::Vec4f clip;
  math::Vec2f uv;
  math::Vec4f color;
};

double distance(math::Vec4f p, int plane) {
  switch (plane) {
  case 0:
    return double(p.x) + p.w;
  case 1:
    return double(p.w) - p.x;
  case 2:
    return double(p.y) + p.w;
  case 3:
    return double(p.w) - p.y;
  case 4:
    return p.z;
  default:
    return double(p.w) - p.z;
  }
}

Vertex interpolate(Vertex a, Vertex b, double t) {
  const auto mix = [t](float x, float y) {
    return float(x + (double(y) - x) * t);
  };
  return {{mix(a.clip.x, b.clip.x), mix(a.clip.y, b.clip.y),
           mix(a.clip.z, b.clip.z), mix(a.clip.w, b.clip.w)},
          {mix(a.uv.x, b.uv.x), mix(a.uv.y, b.uv.y)},
          {mix(a.color.x, b.color.x), mix(a.color.y, b.color.y),
           mix(a.color.z, b.color.z), mix(a.color.w, b.color.w)}};
}

std::vector<Vertex> clip(std::vector<Vertex> polygon) {
  for (int plane = 0; plane < 6 && !polygon.empty(); ++plane) {
    std::vector<Vertex> result;
    auto previous = polygon.back();
    auto previousDistance = distance(previous.clip, plane);
    for (auto current : polygon) {
      const auto currentDistance = distance(current.clip, plane);
      if ((currentDistance >= 0) != (previousDistance >= 0))
        result.push_back(
            interpolate(previous, current,
                        double(previousDistance) /
                            (double(previousDistance) - currentDistance)));
      if (currentDistance >= 0)
        result.push_back(current);
      previous = current;
      previousDistance = currentDistance;
    }
    polygon = std::move(result);
  }
  return polygon;
}

struct ScreenVertex {
  double x, y, depth, inverseW, u, v;
  std::array<double, 4> color;
};

double edge(const ScreenVertex &a, const ScreenVertex &b, double x, double y) {
  return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

bool leading(const ScreenVertex &a, const ScreenVertex &b) {
  return b.y < a.y || (b.y == a.y && b.x > a.x);
}

math::LinearRGBA texel(const SurfacePaintImage &image, int x, int y) {
  math::ColorRGBA8 color;
  if (!SDL_ReadSurfacePixel(image.surface().get(), x, y, &color.r, &color.g,
                            &color.b, &color.a))
    throwSDLError("Cannot read scene texture");
  float r = color.r / 255.f, g = color.g / 255.f, b = color.b / 255.f;
  const float a = color.a / 255.f;
  if (image.isPremultiplied()) {
    r = a > 0 ? std::min(r / a, 1.f) : 0;
    g = a > 0 ? std::min(g / a, 1.f) : 0;
    b = a > 0 ? std::min(b / a, 1.f) : 0;
  }
  if (image.colorEncoding() == rendering::ColorEncoding::SRGB) {
    r = math::decodeSRGB(r);
    g = math::decodeSRGB(g);
    b = math::decodeSRGB(b);
  }
  return {r, g, b, a};
}

math::Vec2i imageSize(const SurfacePaintImage &image) {
  return {image.surface()->w, image.surface()->h};
}

math::Vec2i imageSize(const rendering::Texture &image) {
  return image.levels().front().size;
}

math::LinearRGBA texel(const rendering::Texture &image, int x, int y) {
  const auto &level = image.levels().front();
  const auto p = level.texels[std::size_t(y) * level.size.x + x];
  return {p.x, p.y, p.z, p.w};
}

template <class Image>
math::LinearRGBA sample(const Image &image, double u, double v,
                        const scene::MaterialProps &material) {
  const auto normalized = [](double value, scene::TextureAddress mode) {
    if (mode == scene::TextureAddress::Clamp)
      return std::clamp(value, 0.0, 1.0);
    if (mode == scene::TextureAddress::Repeat)
      return value - std::floor(value);
    double x = std::fmod(value, 2.0);
    if (x < 0)
      x += 2;
    return x <= 1 ? x : 2 - x;
  };
  const auto addressed = [](int index, int extent, scene::TextureAddress mode) {
    if (mode == scene::TextureAddress::Clamp)
      return std::clamp(index, 0, extent - 1);
    const std::int64_t period = mode == scene::TextureAddress::Repeat
                                    ? extent
                                    : std::int64_t(extent) * 2;
    auto wrapped = std::int64_t(index) % period;
    if (wrapped < 0)
      wrapped += period;
    if (wrapped >= extent)
      wrapped = period - 1 - wrapped;
    return int(wrapped);
  };
  const auto size = imageSize(image);
  const int width = size.x, height = size.y;
  u = normalized(u, material.addressU);
  v = normalized(v, material.addressV);
  if (material.sampling == rendering::Sampling::Nearest)
    return texel(image, std::min(int(u * width), width - 1),
                 std::min(int(v * height), height - 1));
  const double x = u * width - .5, y = v * height - .5;
  const int left = int(std::floor(x)), top = int(std::floor(y));
  const float fx = float(x - left), fy = float(y - top);
  math::PremultipliedRGBA result{};
  for (int dy = 0; dy < 2; ++dy)
    for (int dx = 0; dx < 2; ++dx) {
      auto straight =
          texel(image, addressed(left + dx, width, material.addressU),
                addressed(top + dy, height, material.addressV));
      if (material.alpha == scene::MaterialProps::Alpha::Opaque)
        straight.a = 1;
      const auto c = math::premultiply(straight);
      const float weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
      result.r += c.r * weight;
      result.g += c.g * weight;
      result.b += c.b * weight;
      result.a += c.a * weight;
    }
  result.a = std::clamp(result.a, 0.f, 1.f);
  result.r = std::clamp(result.r, 0.f, result.a);
  result.g = std::clamp(result.g, 0.f, result.a);
  result.b = std::clamp(result.b, 0.f, result.a);
  return math::unpremultiply(result);
}
} // namespace

rendering::PaintImageHandle
SoftwareSceneRenderer::render(const scene::SceneRenderProps &view,
                              std::span<const scene::MeshDraw> draws) {
  scene::validate(view, draws);
  if (view.toneMap)
    throw std::invalid_argument("Software scene does not support tone mapping");
  for (const auto &draw : draws)
    if (draw.material.pbr)
      throw std::invalid_argument(
          "Software scene requires explicit unlitPreview, not PBR");
  _limits.validateTarget(view.pixelSize, 4);
  if (std::uint64_t(view.pixelSize.x) * view.pixelSize.y >
      _limits.maxSoftwareTargetPixels)
    throw std::length_error("Software scene exceeds pixel budget");
  const auto count = rendering::RGBA8Image::byteSize(view.pixelSize) / 4;
  std::vector<float> depth(count, 1);
  std::vector<math::PremultipliedRGBA> pixels(
      count, math::premultiply(math::toLinear(view.clearColor)));
  const int width = view.pixelSize.x, height = view.pixelSize.y;

  for (const auto &draw : draws) {
    auto sampling = draw.material;
    const auto &binding = draw.material.colorTexture;
    if (binding.texture) {
      sampling.sampling = binding.sampler.magnification;
      sampling.addressU = binding.sampler.addressU;
      sampling.addressV = binding.sampler.addressV;
    }
    const auto *texture = dynamic_cast<const SurfacePaintImage *>(
        draw.material.baseColorImage.get());
    if (draw.material.baseColorImage && !texture)
      throw std::invalid_argument(
          "Software scene requires CPU-readable material images");
    const auto matrix = view.camera.projection * view.camera.view * draw.model;
    const auto &model = draw.model;
    const double determinant =
        double(model.at(0, 0)) * (double(model.at(1, 1)) * model.at(2, 2) -
                                  double(model.at(1, 2)) * model.at(2, 1)) -
        double(model.at(0, 1)) * (double(model.at(1, 0)) * model.at(2, 2) -
                                  double(model.at(1, 2)) * model.at(2, 0)) +
        double(model.at(0, 2)) * (double(model.at(1, 0)) * model.at(2, 1) -
                                  double(model.at(1, 1)) * model.at(2, 0));
    const auto tint = math::toLinear(draw.material.baseColor);
    const auto &mesh = draw.mesh->data();
    for (std::size_t triangle = 0; triangle < mesh.indices.size();
         triangle += 3) {
      std::vector<Vertex> vertices;
      for (int i = 0; i < 3; ++i) {
        const auto &v = mesh.vertices[mesh.indices[triangle + i]];
        vertices.push_back(
            {matrix * math::Vec4f{v.position.x, v.position.y, v.position.z, 1},
             binding.texture
                 ? binding.transform.apply(binding.uvSet ? v.uv1 : v.uv)
                 : v.uv,
             v.color});
      }
      const auto polygon = clip(std::move(vertices));
      for (std::size_t fan = 1; fan + 1 < polygon.size(); ++fan) {
        std::array<ScreenVertex, 3> screen;
        bool valid = true;
        const std::array inputs{polygon[0], polygon[fan], polygon[fan + 1]};
        for (int i = 0; i < 3; ++i) {
          const auto &v = inputs[i];
          if (v.clip.w <= 0) {
            valid = false;
            break;
          }
          const double inv = 1.0 / v.clip.w;
          screen[i] = {(v.clip.x * inv + 1) * width * .5,
                       (1 - v.clip.y * inv) * height * .5,
                       v.clip.z * inv,
                       inv,
                       v.uv.x * inv,
                       v.uv.y * inv,
                       {v.color.x * inv, v.color.y * inv, v.color.z * inv,
                        v.color.w * inv}};
        }
        if (!valid)
          continue;
        auto area = edge(screen[0], screen[1], screen[2].x, screen[2].y);
        if (area == 0)
          continue;
        if (!draw.material.doubleSided &&
            (determinant < 0 ? area >= 0 : area <= 0))
          continue;
        if (area < 0) {
          std::swap(screen[1], screen[2]);
          area = -area;
        }
        const int left = std::max(
            0,
            int(std::floor(std::min({screen[0].x, screen[1].x, screen[2].x}))));
        const int right = std::min(
            width,
            int(std::ceil(std::max({screen[0].x, screen[1].x, screen[2].x}))));
        const int top = std::max(
            0,
            int(std::floor(std::min({screen[0].y, screen[1].y, screen[2].y}))));
        const int bottom = std::min(
            height,
            int(std::ceil(std::max({screen[0].y, screen[1].y, screen[2].y}))));
        for (int y = top; y < bottom; ++y)
          for (int x = left; x < right; ++x) {
            const std::array weights{
                edge(screen[1], screen[2], x + .5, y + .5),
                edge(screen[2], screen[0], x + .5, y + .5),
                edge(screen[0], screen[1], x + .5, y + .5)};
            bool inside = true;
            for (int i = 0; i < 3; ++i)
              if (weights[i] < 0 ||
                  (weights[i] == 0 &&
                   !leading(screen[(i + 1) % 3], screen[(i + 2) % 3])))
                inside = false;
            if (!inside)
              continue;
            double z{}, inverseW{}, u{}, v{};
            std::array<double, 4> vertexColor{};
            for (int i = 0; i < 3; ++i) {
              const double weight = weights[i] / area;
              z += weight * screen[i].depth;
              inverseW += weight * screen[i].inverseW;
              u += weight * screen[i].u;
              v += weight * screen[i].v;
              for (int c = 0; c < 4; ++c)
                vertexColor[c] += weight * screen[i].color[c];
            }
            const auto index = std::size_t(y) * width + x;
            if (z < 0 || z > 1 || z >= depth[index] || inverseW <= 0)
              continue;
            auto color =
                math::LinearRGBA{tint.r * float(vertexColor[0] / inverseW),
                                 tint.g * float(vertexColor[1] / inverseW),
                                 tint.b * float(vertexColor[2] / inverseW),
                                 tint.a * float(vertexColor[3] / inverseW)};
            if (binding.texture) {
              const auto s = sample(*binding.texture, u / inverseW,
                                    v / inverseW, sampling);
              color = {color.r * s.r, color.g * s.g, color.b * s.b,
                       color.a * s.a};
            }
            if (texture) {
              const auto sampled =
                  sample(*texture, u / inverseW, v / inverseW, draw.material);
              color = {color.r * sampled.r, color.g * sampled.g,
                       color.b * sampled.b, color.a * sampled.a};
            }
            if (draw.material.alpha == scene::MaterialProps::Alpha::Mask &&
                color.a < draw.material.alphaCutoff)
              continue;
            if (draw.material.alpha != scene::MaterialProps::Alpha::Blend) {
              color.a = 1;
              depth[index] = float(z);
            }
            pixels[index] =
                math::sourceOver(math::premultiply(color), pixels[index]);
          }
      }
    }
  }
  SurfaceHandle surface{
      SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32),
      SurfaceHandleDeleter{}};
  if (!surface)
    throwSDLError("Cannot allocate scene output");
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      auto linear = math::unpremultiply(pixels[std::size_t(y) * width + x]);
      linear.r *= view.exposure;
      linear.g *= view.exposure;
      linear.b *= view.exposure;
      const auto color = math::toSRGB(linear);
      if (!SDL_WriteSurfacePixel(surface.get(), x, y, color.r, color.g, color.b,
                                 color.a))
        throwSDLError("Cannot write scene output");
    }
  return makeSurfaceImage(std::move(surface));
}

} // namespace playground::sdl
