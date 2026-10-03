#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include <scene/Environment.hpp>

namespace playground::scene {
namespace {
constexpr float pi = std::numbers::pi_v<float>;
using math::Vec3f;

float radical(unsigned bits) {
  bits = (bits << 16) | (bits >> 16);
  bits = ((bits & 0x55555555u) << 1) | ((bits & 0xaaaaaaaau) >> 1);
  bits = ((bits & 0x33333333u) << 2) | ((bits & 0xccccccccu) >> 2);
  bits = ((bits & 0x0f0f0f0fu) << 4) | ((bits & 0xf0f0f0f0u) >> 4);
  bits = ((bits & 0x00ff00ffu) << 8) | ((bits & 0xff00ff00u) >> 8);
  return bits * 2.3283064365386963e-10f;
}

Vec3f world(Vec3f local, Vec3f n) {
  const auto t = math::normalized(
      math::cross(std::abs(n.y) < .999f ? Vec3f{0, 1, 0} : Vec3f{1, 0, 0}, n));
  return t * local.x + math::cross(n, t) * local.y + n * local.z;
}

Vec3f direction(float u, float v) {
  const float phi = (u - .5f) * 2 * pi, theta = v * pi;
  return {std::sin(phi) * std::sin(theta), std::cos(theta),
          std::cos(phi) * std::sin(theta)};
}

Vec3f sample(const rendering::PackedTextureLevel &s, Vec3f d) {
  const float u = std::atan2(d.x, d.z) / (2 * pi) + .5f,
              v = std::acos(std::clamp(d.y, -1.f, 1.f)) / pi;
  const float px = u * s.size.x - .5f, py = v * s.size.y - .5f;
  const int x = int(std::floor(px)), y = int(std::floor(py));
  Vec3f value{};
  for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i) {
      const int sx = ((x + i) % s.size.x + s.size.x) % s.size.x,
                sy = std::clamp(y + j, 0, s.size.y - 1);
      const auto p = s.texels[std::size_t(sy) * s.size.x + sx];
      const float w = (i ? px - x : 1 - (px - x)) * (j ? py - y : 1 - (py - y));
      value = value + Vec3f{p.x, p.y, p.z} * w;
    }
  return value;
}

Vec3f ggx(float u, float v, float roughness) {
  const float a = roughness * roughness, phi = 2 * pi * u;
  const float z = std::sqrt((1 - v) / (1 + (a * a - 1) * v)),
              r = std::sqrt(std::max(0.f, 1 - z * z));
  return {r * std::cos(phi), r * std::sin(phi), z};
}
} // namespace

void EnvironmentProps::validate() const {
  const auto power = [](unsigned n) { return n && (n & (n - 1)) == 0; };
  if (!power(diffuseWidth) || diffuseWidth < 2 || diffuseWidth > 128 ||
      !power(specularWidth) || specularWidth < 2 || specularWidth > 512 ||
      !brdfSize || brdfSize > 256 || !samples || samples > 4096)
    throw std::invalid_argument("Invalid environment preparation limits");
}

PreparedEnvironment prepareEnvironment(const rendering::Texture &source,
                                       EnvironmentProps props,
                                       std::stop_token stop) {
  props.validate();
  if (source.role() != rendering::TextureRole::Environment)
    throw std::invalid_argument("Environment requires linear HDR texels");
  auto check = [&] {
    if (stop.stop_requested())
      throw std::runtime_error("Environment preparation canceled");
  };
  const auto &src = source.levels().front();
  std::vector<rendering::TextureLevel> specular;
  const unsigned count = unsigned(std::log2(props.specularWidth)) + 1;
  rendering::TextureLevel diffuse{
      {int(props.diffuseWidth), int(props.diffuseWidth / 2)}, {}};
  auto filter = [&](rendering::TextureLevel &level, float roughness,
                    bool diffuseFilter) {
    level.texels.resize(std::size_t(level.size.x) * level.size.y);
    for (int y = 0; y < level.size.y; ++y) {
      check();
      for (int x = 0; x < level.size.x; ++x) {
        const auto n =
            direction((x + .5f) / level.size.x, (y + .5f) / level.size.y);
        Vec3f sum{};
        float weight{};
        for (unsigned i = 0; i < props.samples; ++i) {
          const float u = (i + .5f) / props.samples, v = radical(i);
          Vec3f l;
          if (diffuseFilter)
            l = world({std::sqrt(v) * std::cos(2 * pi * u),
                       std::sqrt(v) * std::sin(2 * pi * u), std::sqrt(1 - v)},
                      n);
          else {
            const auto h = world(ggx(u, v, std::max(.001f, roughness)), n);
            l = h * (2 * math::dot(n, h)) - n;
          }
          const float w = diffuseFilter ? 1.f : std::max(0.f, math::dot(n, l));
          sum = sum + sample(src, l) * w;
          weight += w;
        }
        if (weight > 0)
          sum = sum * (1 / weight);
        level.texels[std::size_t(y) * level.size.x + x] = {sum.x, sum.y, sum.z,
                                                           1};
      }
    }
  };
  filter(diffuse, 1, true);
  for (unsigned level = 0; level < count; ++level) {
    rendering::TextureLevel image{
        {std::max(1, int(props.specularWidth >> level)),
         std::max(1, int((props.specularWidth / 2) >> level))},
        {}};
    filter(image, float(level) / (count - 1), false);
    specular.push_back(std::move(image));
  }
  rendering::TextureLevel brdf{{int(props.brdfSize), int(props.brdfSize)}, {}};
  brdf.texels.resize(std::size_t(props.brdfSize) * props.brdfSize);
  for (unsigned y = 0; y < props.brdfSize; ++y) {
    check();
    for (unsigned x = 0; x < props.brdfSize; ++x) {
      const float nv = (x + .5f) / props.brdfSize,
                  r = (y + .5f) / props.brdfSize;
      const Vec3f v{std::sqrt(1 - nv * nv), 0, nv};
      float a{}, b{};
      for (unsigned i = 0; i < props.samples; ++i) {
        const auto h = ggx((i + .5f) / props.samples, radical(i), r);
        const float vh = std::max(0.f, math::dot(v, h));
        const auto l = h * (2 * vh) - v;
        const float nl = std::max(0.f, l.z), nh = std::max(0.f, h.z);
        if (nl > 0 && nh > 0) {
          const float alpha = r * r, k = alpha * .5f;
          const float g = (nv / (nv * (1 - k) + k)) * (nl / (nl * (1 - k) + k));
          const float visibility = g * vh / (nh * nv),
                      f = std::pow(1 - vh, 5.f);
          a += (1 - f) * visibility;
          b += f * visibility;
        }
      }
      brdf.texels[std::size_t(y) * props.brdfSize + x] = {
          a / props.samples, b / props.samples, 0, 1};
    }
  }
  return {rendering::makeTexture(std::move(diffuse),
                                 rendering::TextureRole::Environment,
                                 rendering::MipPolicy::None, 256 * 1024 * 1024,
                                 source.resources()),
          std::make_shared<const rendering::Texture>(
              rendering::TextureRole::Environment, std::move(specular),
              source.resources()),
          rendering::makeTexture(std::move(brdf), rendering::TextureRole::Data,
                                 rendering::MipPolicy::None, 256 * 1024 * 1024,
                                 source.resources())};
}
} // namespace playground::scene
