#pragma once

#include <functional>

#include <platform/sdl/GPUResources.hpp>
#include <rendering/RenderBackend.hpp>

namespace playground::sdl {

struct GPUOffscreenProps {
  math::Vec2i size;
  math::ColorRGBA8 clearColor{};
  bool depth{};
};

struct GPURecordingContext {
  SDL_GPUCommandBuffer *commands;
  SDL_GPURenderPass *pass;
  GPUDevice &device;

  // Declare every sampled project image before issuing native draw calls.
  // This keeps pooled textures leased through recording and GPU completion.
  void use(const GPUImage &image) const {
    device.recordUse(commands, image._texture.use());
  }
};

// Optional SDL adapter extension; neutral frames/UI never depend on native
// types. Native handles are callback-only borrows. Do not end/submit the
// supplied pass or commands. Output is immutable RGBA16 linear-premultiplied;
// depth is D32.
class GPUFrameAccess {
public:
  virtual ~GPUFrameAccess() = default;
  virtual GPUDeviceHandle gpuDevice() const = 0;
  virtual rendering::PaintImageHandle
  renderOffscreen(GPUOffscreenProps props,
                  const std::function<void(GPURecordingContext)> &record) = 0;
};

inline GPUFrameAccess *gpuAccess(rendering::RenderFrame &frame) noexcept {
  return dynamic_cast<GPUFrameAccess *>(&frame);
}

} // namespace playground::sdl
