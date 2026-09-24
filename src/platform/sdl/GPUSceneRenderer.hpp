#pragma once

#include <array>
#include <map>

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

  PaintDevice &_device;
  Shader _vertex, _fragment;
  std::array<Pipeline, 9> _pipelines;
  Sampler _sampler;
  UploadStream _uploads;
  std::size_t _residentBytes{};
  std::uint64_t _clock{};
  std::map<std::weak_ptr<const scene::Mesh>, MeshEntry,
           std::owner_less<std::weak_ptr<const scene::Mesh>>>
      _meshes;

  std::shared_ptr<const Mesh> mesh(scene::MeshHandle source);
  void pruneMeshes();

public:
  explicit GPUSceneRenderer(PaintDevice &device);
  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return _device.device->resourceDomain();
  }
  std::size_t residentBytes() const noexcept { return _residentBytes; }
  rendering::PaintImageHandle
  render(const scene::SceneRenderProps &view,
         std::span<const scene::MeshDraw> draws) override;
};

} // namespace playground::sdl::gpu_detail
