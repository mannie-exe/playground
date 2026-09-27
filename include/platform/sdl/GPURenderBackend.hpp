#pragma once

#include <platform/sdl/GPUResources.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderBackendProps.hpp>

namespace playground::sdl {

// Compiled shader formats are independent of runtime driver availability.
SDL_GPUShaderFormat packagedShaderFormats() noexcept;
std::vector<rendering::RendererCandidate> availableGPURenderers();

class GPURenderBackend final : public rendering::RenderBackend {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  GPURenderBackend(SDL_Window &window, rendering::GPUDriver driver,
                   const rendering::RenderBackendProps &props = {});
  ~GPURenderBackend() override;
  rendering::ResourceDomainId resourceDomain() const noexcept override;
  std::uint64_t completedWork() override;
  void invalidate() noexcept override;
  void setProfilingEnabled(bool enabled) override;
  bool supportsGPUTiming() const noexcept override;
  std::vector<rendering::GPUTimingSample> takeGPUTimings() override;
  rendering::GPUTimingCollection gpuTimingCollection() const override;
  std::optional<rendering::PaintWork> takePaintWork() override;
  rendering::RendererCandidate description() const override;
  void prepare(rendering::RendererRequirements requirements) override;
  math::Vec2i drawableSize() const override;
  std::unique_ptr<rendering::RenderFrame>
  beginFrame(rendering::RenderFrameProps props) override;
};

} // namespace playground::sdl
