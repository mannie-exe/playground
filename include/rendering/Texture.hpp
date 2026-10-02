#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include <math/Geometry3D.hpp>
#include <rendering/ImageData.hpp>
#include <rendering/PaintImage.hpp>
#include <rendering/ResourceLedger.hpp>

namespace playground::rendering {

enum class TextureRole { Color, Emission, Normal, Data, Environment };
enum class MipPolicy { None, Generate, Provided };
enum class TextureAddress { Clamp, Repeat, MirroredRepeat };
enum class MipFilter { None, Nearest, Linear };
enum class TextureFormat { RGBA8, RGBA16F, RGBA32F, R8 };
std::size_t texelBytes(TextureFormat format);

// Compact storage; reads decode linear working values without changing alpha
// association. The owning Texture declares that association.
class PackedTexels {
  TextureFormat _format;
  ColorEncoding _encoding;
  ResourceLedger::Token _allocation;
  std::vector<std::byte> _data;

public:
  PackedTexels(TextureFormat format, ColorEncoding encoding,
               std::vector<std::byte> data);
  PackedTexels(TextureFormat format, ColorEncoding encoding,
               std::span<const math::Vec4f> linear);

  PackedTexels(const PackedTexels &other);
  PackedTexels(PackedTexels &&) noexcept = default;
  PackedTexels &operator=(const PackedTexels &other);
  PackedTexels &operator=(PackedTexels &&other) noexcept;

  TextureFormat format() const noexcept { return _format; }

  ColorEncoding encoding() const noexcept { return _encoding; }

  std::span<const std::byte> data() const noexcept { return _data; }

  std::size_t size() const noexcept;
  math::Vec4f operator[](std::size_t index) const;

  struct Iterator {
    using value_type = math::Vec4f;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;
    const PackedTexels *source{};
    std::size_t index{};

    math::Vec4f operator*() const { return (*source)[index]; }

    Iterator &operator++() {
      ++index;
      return *this;
    }

    Iterator operator++(int) {
      auto old = *this;
      ++*this;
      return old;
    }

    bool operator==(const Iterator &) const = default;
  };

  Iterator begin() const { return {this, 0}; }

  Iterator end() const { return {this, size()}; }

  bool operator==(const PackedTexels &other) const;
};

struct SamplerProps {
  Sampling minification{Sampling::Linear}, magnification{Sampling::Linear};
  MipFilter mip{MipFilter::Linear};
  TextureAddress addressU{TextureAddress::Repeat},
      addressV{TextureAddress::Repeat};
  float anisotropy{1};
  void validate() const;
  bool operator==(const SamplerProps &) const = default;
};

struct TextureLevel {
  math::Vec2i size;
  std::vector<math::Vec4f> texels;
};

struct PackedTextureLevel {
  math::Vec2i size;
  PackedTexels texels;
};

// Material sources use straight alpha; renderer-prepared color copies may be
// associated. Reads decode color to linear light. Normal/data stay numerical.
class Texture final {
  TextureRole _role;
  AlphaMode _alpha{AlphaMode::Straight};
  std::vector<PackedTextureLevel> _levels;
  std::size_t _bytes{};
  bool _opaque{true};
  mutable std::mutex _uploadMutex;
  mutable std::shared_ptr<const Texture> _opaqueUpload, _associatedUpload;

public:
  Texture(TextureRole role, std::vector<TextureLevel> levels);
  Texture(TextureRole role, std::vector<PackedTextureLevel> levels,
          AlphaMode alpha = AlphaMode::Straight);

  Texture(const Texture &other)
      : _role{other._role}, _alpha{other._alpha}, _levels{other._levels},
        _bytes{other._bytes}, _opaque{other._opaque} {}

  Texture &operator=(const Texture &) = delete;
  Texture &operator=(Texture &&) = delete;

  bool opaque() const noexcept { return _opaque; }

  const Texture &upload(bool ignoreAlpha) const;

  TextureRole role() const noexcept { return _role; }

  AlphaMode alphaMode() const noexcept { return _alpha; }

  const std::vector<PackedTextureLevel> &levels() const noexcept {
    return _levels;
  }

  std::size_t bytes() const noexcept { return _bytes; }
};

using TextureHandle = std::shared_ptr<const Texture>;

TextureHandle makeTexture(TextureLevel level, TextureRole role,
                          MipPolicy policy = MipPolicy::Generate,
                          std::size_t maximumBytes = 256 * 1024 * 1024);
TextureHandle makeTexture(PackedTextureLevel level, TextureRole role,
                          MipPolicy policy = MipPolicy::Generate,
                          std::size_t maximumBytes = 256 * 1024 * 1024);
TextureHandle makeTexture(const RGBA8Image &image, TextureRole role,
                          MipPolicy policy = MipPolicy::Generate,
                          std::size_t maximumBytes = 256 * 1024 * 1024);
// Preserve base-level coverage approximately in discrete lower mip levels.
TextureHandle preserveAlphaCoverage(TextureHandle source, float cutoff);
// Ignore coverage before filtering. Rebuild alpha-weighted mips from level
// zero, preserving the source level count; already-opaque provided chains are
// retained.
TextureHandle makeOpaqueTexture(const Texture &source);
// Explicit conversion; out-of-range values fail rather than clamp. Input float
// textures retain float32 unless a preparation path selects compact storage.
TextureHandle packTexture(const Texture &source, TextureFormat format,
                          ColorEncoding encoding = ColorEncoding::Linear,
                          bool premultiply = false,
                          std::size_t maximumBytes = 256 * 1024 * 1024);

struct UVTransform {
  // Binding-local offset + rotation * scale; rotation is in radians.
  math::Vec2f offset{}, scale{1, 1};
  float rotation{};
  math::Vec2f apply(math::Vec2f uv) const;
  bool operator==(const UVTransform &) const = default;
};

struct TextureBinding {
  TextureHandle texture;
  SamplerProps sampler;
  unsigned uvSet{};
  UVTransform transform;
  void validate() const;
};

} // namespace playground::rendering
