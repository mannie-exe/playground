#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "GPUPainter.hpp"
#include <scene/SceneRenderer.hpp>

namespace playground::sdl::gpu_detail {

class GPUSceneRenderer final : public scene::SceneRenderer {
  struct Mesh {
    Buffer vertices, indices;
    Uint32 count;
  };

  struct MeshEntry {
    std::shared_ptr<const Mesh> mesh;
    std::size_t bytes{};
    std::uint64_t lastUse{};
  };

  struct TextureEntry {
    std::shared_ptr<GPUTextureData> texture;
    std::size_t bytes{};
    std::uint64_t lastUse{};
  };

  struct TextureKey {
    std::weak_ptr<const rendering::Texture> source;
    bool ignoreAlpha{};

    bool operator<(const TextureKey &other) const noexcept {
      if (source.owner_before(other.source))
        return true;
      if (other.source.owner_before(source))
        return false;
      return ignoreAlpha < other.ignoreAlpha;
    }
  };

  PaintDevice &_device;
  Shader _vertex, _fragment;
  std::array<Pipeline, 9> _pipelines;
  Shader _pbrVertex, _pbrFragment, _toneVertex, _toneFragment;
  std::array<Pipeline, 9> _pbrPipelines;
  Pipeline _tonePipeline;
  std::map<TextureKey, TextureEntry> _textures;
  std::vector<std::pair<rendering::SamplerProps, Sampler>> _samplers;
  std::size_t _textureBytes{};
  rendering::TextureHandle _whiteTexture, _normalTexture, _blackTexture;
  Sampler _sampler;
  UploadStream _uploads;
  std::size_t _residentBytes{};
  std::uint64_t _clock{};
  std::map<std::weak_ptr<const scene::Mesh>, MeshEntry,
           std::owner_less<std::weak_ptr<const scene::Mesh>>>
      _meshes;

  std::shared_ptr<const Mesh> mesh(scene::MeshHandle source);
  std::shared_ptr<GPUTextureData>
  texture(const rendering::TextureHandle &source, bool ignoreAlpha = false);
  SDL_GPUSampler *textureSampler(const rendering::SamplerProps &props);
  void pruneResources();

public:
  explicit GPUSceneRenderer(PaintDevice &device);

  void trimUnused() {
    _meshes.clear();
    _residentBytes = 0;
    _textures.clear();
    _textureBytes = 0;
    _uploads.trim();
  }

  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return _device.device->resourceDomain();
  }

  std::size_t meshResidentBytes() const noexcept { return _residentBytes; }

  std::size_t textureResidentBytes() const noexcept { return _textureBytes; }

  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override;
};

} // namespace playground::sdl::gpu_detail
