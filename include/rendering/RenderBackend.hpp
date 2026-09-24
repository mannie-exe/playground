#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <rendering/GPUTiming.hpp>
#include <rendering/PaintContext.hpp>
#include <rendering/RenderSettings.hpp>
#include <rendering/RendererTypes.hpp>
#include <rendering/ResourceDomain.hpp>
#include <scene/SceneRenderer.hpp>

namespace playground::rendering {

// Submitted is queue acceptance, not completion. Skipped includes an
// unavailable swapchain; neither outcome proves GPU execution completed.
enum class PresentationOutcome { Submitted, Skipped };

struct RenderFrameProps {
  math::ColorRGBA8 clearColor;
  RenderSettings settings;
};

// A frame borrows its backend. Destroy it before the backend/window, and before
// resizing the target. Destruction abandons unfinished application recording;
// it cannot roll back submitted work. GPU present() must acquire the swapchain
// late and handle post-acquisition cleanup without attempting cancellation.
class RenderFrame {
protected:
  RenderFrame() = default;

public:
  virtual ~RenderFrame() = default;
  virtual rendering::PaintContext &paint2D() = 0;
  // Optional capability, not a no-op renderer. Check requirements before entry.
  virtual scene::SceneRenderer *scene3D() noexcept { return nullptr; }
  virtual PresentationOutcome present() = 0;

  RenderFrame(const RenderFrame &) = delete;
  RenderFrame &operator=(const RenderFrame &) = delete;
};

class RenderBackend {
protected:
  RenderBackend() = default;

public:
  virtual ~RenderBackend() = default;
  virtual RendererCandidate description() const = 0;
  virtual ResourceDomainId resourceDomain() const noexcept {
    return ResourceDomainId::cpu();
  }
  // Owner-thread, nonblocking completion polling. Sequence belongs to this
  // backend instance; never infer completion from a successful submission.
  virtual std::uint64_t completedWork() { return 0; }
  // Failed native domains reject new work even while retained handles survive.
  virtual void invalidate() noexcept {}
  virtual bool supportsGPUTiming() const noexcept { return false; }
  virtual void setProfilingEnabled(bool) {}
  virtual std::vector<GPUTimingSample> takeGPUTimings() { return {}; }
  // Realize required optional services before publishing a backend/app change.
  // Optional capabilities may stay lazy until requested by a frame.
  virtual void prepare(RendererRequirements requirements) {
    if (!description().capabilities.supports(requirements))
      throw std::runtime_error("Renderer cannot satisfy required capabilities");
  }
  virtual math::Vec2i drawableSize() const = 0;
  // Only one live frame at a time. Null means the target is temporarily
  // unavailable; update/input continue, but this frame must not be painted.
  virtual std::unique_ptr<RenderFrame> beginFrame(RenderFrameProps props) = 0;

  RenderBackend(const RenderBackend &) = delete;
  RenderBackend &operator=(const RenderBackend &) = delete;
};

} // namespace playground::rendering
