#pragma once

#include <memory>

#include <math/Color.hpp>
#include <math/Geometry2D.hpp>
#include <platform/Presentation.hpp>
#include <scene/SceneRenderer.hpp>
#include <ui/PaintContext.hpp>

namespace playground::rendering {

struct RenderFrameProps {
  math::ColorRGBA8 clearColor;
  platform::RenderSettings settings;
};

// A frame borrows its backend. Destroy it before the backend/window, and before
// resizing the target. Destruction abandons unfinished work without presenting.
class RenderFrame {
protected:
  RenderFrame() = default;

public:
  virtual ~RenderFrame() = default;
  virtual ui::PaintContext &paint2D() = 0;
  // Optional capability, not a no-op renderer. Software frames return null.
  virtual scene::SceneRenderer *scene3D() noexcept { return nullptr; }
  virtual void present() = 0;

  RenderFrame(const RenderFrame &) = delete;
  RenderFrame &operator=(const RenderFrame &) = delete;
};

class RenderBackend {
protected:
  RenderBackend() = default;

public:
  virtual ~RenderBackend() = default;
  virtual math::Vec2i drawableSize() const = 0;
  // Only one live frame at a time. Null means the target is temporarily
  // unavailable; update/input continue, but this frame must not be painted.
  virtual std::unique_ptr<RenderFrame> beginFrame(RenderFrameProps props) = 0;

  RenderBackend(const RenderBackend &) = delete;
  RenderBackend &operator=(const RenderBackend &) = delete;
};

} // namespace playground::rendering
