#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <rendering/Texture.hpp>
#include <support/PreparationBudget.hpp>

namespace playground::rendering {
namespace {
constexpr std::size_t conversionBlockTexels = 4096;

std::size_t count(math::Vec2i s) { return RGBA8Image::byteSize(s) / 4; }

bool valid(TextureRole r) {
  return r == TextureRole::Color || r == TextureRole::Emission ||
         r == TextureRole::Normal || r == TextureRole::Data ||
         r == TextureRole::Environment;
}

std::vector<PackedTextureLevel> packLevels(std::vector<TextureLevel> input) {
  std::vector<PackedTextureLevel> result;
  for (auto &level : input)
    result.push_back(
        {level.size, PackedTexels{TextureFormat::RGBA32F, ColorEncoding::Linear,
                                  std::span{level.texels}}});
  return result;
}

template <class Transform>
PackedTexels transformTexels(const PackedTexels &source, TextureFormat format,
                             ColorEncoding encoding, Transform transform) {
  const auto stride = texelBytes(format);
  if (source.size() > std::numeric_limits<std::size_t>::max() / stride)
    throw std::length_error("Texture conversion size overflow");
  std::vector<std::byte> result(source.size() * stride);
  std::array<math::Vec4f, conversionBlockTexels> block;
  for (std::size_t i = 0; i < source.size(); i += block.size()) {
    const auto count = std::min(block.size(), source.size() - i);
    for (std::size_t j = 0; j < count; ++j)
      block[j] = transform(source[i + j]);
    const PackedTexels packed{format, encoding, std::span{block.data(), count}};
    std::memcpy(result.data() + i * stride, packed.data().data(),
                count * stride);
  }
  return {format, encoding, std::move(result)};
}
} // namespace

void SamplerProps::validate() const {
  const auto filter = [](Sampling f) {
    return f == Sampling::Nearest || f == Sampling::Linear;
  };
  const auto address = [](TextureAddress a) {
    return a == TextureAddress::Clamp || a == TextureAddress::Repeat ||
           a == TextureAddress::MirroredRepeat;
  };
  if (!filter(minification) || !filter(magnification) || !address(addressU) ||
      !address(addressV) ||
      (mip != MipFilter::None && mip != MipFilter::Nearest &&
       mip != MipFilter::Linear) ||
      !std::isfinite(anisotropy) || anisotropy < 1 || anisotropy > 16)
    throw std::invalid_argument("Invalid texture sampler");
}

Texture::Texture(TextureRole role, std::vector<TextureLevel> levels)
    : Texture{role, packLevels(std::move(levels))} {}

Texture::Texture(TextureRole role, std::vector<PackedTextureLevel> levels,
                 AlphaMode alpha)
    : _role{role}, _alpha{alpha}, _levels{std::move(levels)} {
  if (!valid(role) || _levels.empty())
    throw std::invalid_argument("Texture requires a role and level");
  auto expected = _levels.front().size;
  for (std::size_t i = 0; i < _levels.size(); ++i) {
    const auto &level = _levels[i];
    if (level.size != expected || level.texels.size() != count(level.size) ||
        level.texels.data().size() >
            std::numeric_limits<std::size_t>::max() - _bytes ||
        level.texels.format() != _levels.front().texels.format() ||
        level.texels.encoding() != _levels.front().texels.encoding() ||
        (alpha != AlphaMode::Straight && alpha != AlphaMode::Premultiplied) ||
        (alpha == AlphaMode::Premultiplied && role != TextureRole::Color) ||
        (level.texels.encoding() == ColorEncoding::SRGB &&
         role != TextureRole::Color && role != TextureRole::Emission))
      throw std::invalid_argument("Invalid texture mip chain");
    if (level.texels.format() == TextureFormat::RGBA8) {
      const auto data = level.texels.data();
      for (std::size_t j = 3; j < data.size(); j += 4)
        _opaque &= data[j] == std::byte{255};
    } else
      for (auto p : level.texels) {
        _opaque &= p.w == 1;
        if (!math::isFinite(p))
          throw std::invalid_argument("Nonfinite texture texel");
        if (role != TextureRole::Data &&
            (p.x < 0 || p.y < 0 || p.z < 0 || p.w < 0 || p.w > 1))
          throw std::invalid_argument("Invalid color/normal texture range");
        if (role == TextureRole::Normal && (p.x > 1 || p.y > 1 || p.z > 1))
          throw std::invalid_argument(
              "Normal texture must be encoded in unit channels");
      }
    _bytes += level.texels.data().size();
    if (expected == math::Vec2i{1, 1} && i + 1 != _levels.size())
      throw std::invalid_argument("Excess texture mip levels");
    expected = {std::max(1, expected.x / 2), std::max(1, expected.y / 2)};
  }
}

namespace {
TextureHandle buildTexture(PackedTextureLevel level, TextureRole role,
                           MipPolicy policy, std::size_t maximumBytes,
                           std::size_t maximumLevels) {
  if (!valid(role) ||
      (policy != MipPolicy::None && policy != MipPolicy::Generate))
    throw std::invalid_argument(
        "Use Texture constructor for provided mip chains");
  if (level.texels.size() != count(level.size) ||
      level.texels.data().size() > maximumBytes)
    throw std::length_error("Invalid or excessive texture storage");
  std::vector<PackedTextureLevel> levels;
  const auto format = level.texels.format();
  const auto encoding = level.texels.encoding();
  const auto stride = texelBytes(format);
  auto bytes = level.texels.data().size();
  levels.push_back(std::move(level));
  while (policy == MipPolicy::Generate && levels.size() < maximumLevels &&
         levels.back().size != math::Vec2i{1, 1}) {
    const auto &src = levels.back();
    TextureLevel dst{{std::max(1, src.size.x / 2), std::max(1, src.size.y / 2)},
                     {}};
    if (count(dst.size) > (maximumBytes - bytes) / stride)
      throw std::length_error("Mip chain exceeds texture budget");
    dst.texels.resize(count(dst.size));
    bytes += dst.texels.size() * stride;
    for (int y = 0; y < dst.size.y; ++y)
      for (int x = 0; x < dst.size.x; ++x) {
        // Area filter accounts for every source texel, including odd
        // dimensions.
        const double x0 = double(x) * src.size.x / dst.size.x,
                     x1 = double(x + 1) * src.size.x / dst.size.x;
        const double y0 = double(y) * src.size.y / dst.size.y,
                     y1 = double(y + 1) * src.size.y / dst.size.y;
        double sum[4]{}, weight{};
        for (int sy = int(y0); sy < int(std::ceil(y1)); ++sy)
          for (int sx = int(x0); sx < int(std::ceil(x1)); ++sx) {
            const double w =
                (std::min(x1, double(sx + 1)) - std::max(x0, double(sx))) *
                (std::min(y1, double(sy + 1)) - std::max(y0, double(sy)));
            auto p = src.texels[std::size_t(sy) * src.size.x + sx];
            if (role == TextureRole::Color) {
              p.x *= p.w;
              p.y *= p.w;
              p.z *= p.w;
            }
            sum[0] += p.x * w;
            sum[1] += p.y * w;
            sum[2] += p.z * w;
            sum[3] += p.w * w;
            weight += w;
          }
        math::Vec4f p{float(sum[0] / weight), float(sum[1] / weight),
                      float(sum[2] / weight), float(sum[3] / weight)};
        if (role == TextureRole::Color && p.w > 0) {
          p.x /= p.w;
          p.y /= p.w;
          p.z /= p.w;
        }
        if (role == TextureRole::Normal) {
          math::Vec3f n{p.x * 2 - 1, p.y * 2 - 1, p.z * 2 - 1};
          const auto length = std::sqrt(math::dot(n, n));
          n = length > 1e-8f ? n * (1 / length) : math::Vec3f{0, 0, 1};
          p.x = n.x * .5f + .5f;
          p.y = n.y * .5f + .5f;
          p.z = n.z * .5f + .5f;
        }
        dst.texels[std::size_t(y) * dst.size.x + x] = p;
      }
    levels.push_back(
        {dst.size, PackedTexels{format, encoding, std::span{dst.texels}}});
  }
  return std::make_shared<const Texture>(role, std::move(levels));
}
} // namespace

TextureHandle makeTexture(TextureLevel level, TextureRole role,
                          MipPolicy policy, std::size_t maximumBytes) {
  if (level.texels.size() > maximumBytes / sizeof(math::Vec4f))
    throw std::length_error("Texture exceeds preparation budget");
  PackedTextureLevel packed{level.size, PackedTexels{TextureFormat::RGBA32F,
                                                     ColorEncoding::Linear,
                                                     std::span{level.texels}}};
  return buildTexture(std::move(packed), role, policy, maximumBytes,
                      std::numeric_limits<std::size_t>::max());
}

TextureHandle makeOpaqueTexture(const Texture &source) {
  if (source.role() != TextureRole::Color ||
      source.alphaMode() != AlphaMode::Straight)
    throw std::invalid_argument("Opaque preparation requires a color texture");
  if (source.opaque())
    return std::make_shared<const Texture>(source);
  const auto &first = source.levels().front();
  PackedTextureLevel base{first.size,
                          transformTexels(first.texels, first.texels.format(),
                                          first.texels.encoding(), [](auto p) {
                                            p.w = 1;
                                            return p;
                                          })};
  return buildTexture(std::move(base), TextureRole::Color, MipPolicy::Generate,
                      source.bytes(), source.levels().size());
}

TextureHandle makeTexture(PackedTextureLevel level, TextureRole role,
                          MipPolicy policy, std::size_t maximumBytes) {
  return buildTexture(std::move(level), role, policy, maximumBytes,
                      std::numeric_limits<std::size_t>::max());
}

TextureHandle makeTexture(const RGBA8Image &image, TextureRole role,
                          MipPolicy policy, std::size_t maximumBytes) {
  image.validate();
  if (image.alpha != AlphaMode::Straight)
    throw std::invalid_argument("Texture decode requires straight channels");
  if (image.pixels.size() > maximumBytes)
    throw std::length_error("Decoded texture exceeds budget");
  std::vector<std::byte> data(image.pixels.size());
  std::transform(image.pixels.begin(), image.pixels.end(), data.begin(),
                 [](auto value) { return std::byte{value}; });
  if (role == TextureRole::Emission)
    for (std::size_t i = 3; i < data.size(); i += 4)
      data[i] = std::byte{255};
  const auto encoding =
      role == TextureRole::Color || role == TextureRole::Emission
          ? image.encoding
          : ColorEncoding::Linear;
  return buildTexture({image.size, PackedTexels{TextureFormat::RGBA8, encoding,
                                                std::move(data)}},
                      role, policy, maximumBytes,
                      std::numeric_limits<std::size_t>::max());
}

math::Vec2f UVTransform::apply(math::Vec2f uv) const {
  const auto x = uv.x * scale.x, y = uv.y * scale.y;
  return {offset.x + std::cos(rotation) * x - std::sin(rotation) * y,
          offset.y + std::sin(rotation) * x + std::cos(rotation) * y};
}

void TextureBinding::validate() const {
  sampler.validate();
  if (texture && texture->alphaMode() != AlphaMode::Straight)
    throw std::invalid_argument(
        "Material bindings require straight source textures");
  if (uvSet > 1 || !math::isFinite(transform.offset) ||
      !math::isFinite(transform.scale) || !std::isfinite(transform.rotation))
    throw std::invalid_argument("Invalid texture UV binding");
}

TextureHandle preserveAlphaCoverage(TextureHandle source, float cutoff) {
  if (!source || source->role() != TextureRole::Color ||
      source->alphaMode() != AlphaMode::Straight || !std::isfinite(cutoff) ||
      cutoff < 0 || cutoff > 1)
    throw std::invalid_argument(
        "Coverage requires a color texture and unit cutoff");
  if (cutoff == 0 || source->levels().size() == 1)
    return source;
  auto levels = source->levels();
  const auto coverage = [&](const PackedTextureLevel &level, float scale) {
    return double(std::count_if(
               level.texels.begin(), level.texels.end(),
               [&](auto p) { return std::min(p.w * scale, 1.f) >= cutoff; })) /
           level.texels.size();
  };
  const auto desired = coverage(levels.front(), 1);
  for (std::size_t i = 1; i < levels.size(); ++i) {
    auto &level = levels[i];
    float low = 0, high = 16, best = 1;
    double error = std::abs(coverage(level, 1) - desired);
    for (int iteration = 0; iteration < 24; ++iteration) {
      const float scale = (low + high) * .5f;
      const double actual = coverage(level, scale),
                   difference = std::abs(actual - desired);
      if (difference < error) {
        error = difference;
        best = scale;
      }
      if (actual < desired)
        low = scale;
      else
        high = scale;
    }
    level.texels = transformTexels(level.texels, level.texels.format(),
                                   level.texels.encoding(), [best](auto p) {
                                     p.w = std::clamp(p.w * best, 0.f, 1.f);
                                     return p;
                                   });
  }
  return std::make_shared<const Texture>(source->role(), std::move(levels));
}

TextureHandle packTexture(const Texture &source, TextureFormat format,
                          ColorEncoding encoding, bool premultiply,
                          std::size_t maximumBytes) {
  if (source.alphaMode() != AlphaMode::Straight ||
      (premultiply && source.role() != TextureRole::Color))
    throw std::invalid_argument(
        "Texture packing requires straight compatible input");
  std::vector<PackedTextureLevel> levels;
  std::size_t bytes{};
  for (const auto &level : source.levels()) {
    if (level.texels.size() > (maximumBytes - bytes) / texelBytes(format))
      throw std::length_error("Packed texture exceeds budget");
    bytes += level.texels.size() * texelBytes(format);
    levels.push_back(
        {level.size,
         transformTexels(level.texels, format, encoding, [premultiply](auto p) {
           if (premultiply) {
             p.x *= p.w;
             p.y *= p.w;
             p.z *= p.w;
           }
           return p;
         })});
  }
  return std::make_shared<const Texture>(source.role(), std::move(levels),
                                         premultiply ? AlphaMode::Premultiplied
                                                     : AlphaMode::Straight);
}
} // namespace playground::rendering

namespace playground::rendering {
const Texture &Texture::upload(bool ignoreAlpha) const {
  if (_role != TextureRole::Color || _opaque ||
      _alpha == AlphaMode::Premultiplied)
    return *this;
  std::lock_guard lock{_uploadMutex};
  auto &cached = ignoreAlpha ? _opaqueUpload : _associatedUpload;
  if (!cached) {
    const auto &base = _levels.front();
    auto temporaryBytes = base.texels.data().size();
    if (ignoreAlpha && _levels.size() > 1) {
      const auto next =
          count({std::max(1, base.size.x / 2), std::max(1, base.size.y / 2)});
      if (next > std::numeric_limits<std::size_t>::max() / sizeof(math::Vec4f))
        throw std::length_error("Texture conversion scratch overflow");
      temporaryBytes = std::max(temporaryBytes, next * sizeof(math::Vec4f));
    }
    constexpr auto blockBytes = conversionBlockTexels * sizeof(math::Vec4f);
    if (temporaryBytes > std::numeric_limits<std::size_t>::max() - blockBytes)
      throw std::length_error("Texture conversion scratch overflow");
    // Admission precedes the unadopted byte buffer and float mip scratch.
    // Published PackedTexels retain their separate persistent ledger charges.
    const auto scratch =
        resourcePreparationBudget().acquire(temporaryBytes + blockBytes);
    const auto &first = base.texels;
    cached = ignoreAlpha ? makeOpaqueTexture(*this)
                         : packTexture(*this, first.format(), first.encoding(),
                                       true, _bytes);
  }
  return *cached;
}
} // namespace playground::rendering
