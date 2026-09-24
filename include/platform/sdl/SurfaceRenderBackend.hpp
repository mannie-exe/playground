#pragma once

#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <rendering/RenderBackend.hpp>
#include <support/SDLResource.hpp>

namespace playground::sdl {

// Borrows a window; acquires its current surface anew for each frame.
// UI-thread-only. The window must not be resized while a frame is alive.
class SurfaceRenderBackend final : public rendering::RenderBackend {
  SDL_Window &_window;

  bool _frameActive{};
  std::uint64_t _completedWork{};
  SDLResource<SDL_Surface, SDL_DestroySurface> _scaledTarget;

public:
  explicit SurfaceRenderBackend(SDL_Window &window) : _window{window} {}

  static rendering::RendererCandidate availableDescription() {
    return {.capabilities = {.paint2D = true, .scene3D = true}};
  }
  rendering::RendererCandidate description() const override {
    return availableDescription();
  }

  math::Vec2i drawableSize() const override;
  std::uint64_t completedWork() override { return _completedWork; }
  std::unique_ptr<rendering::RenderFrame>
  beginFrame(rendering::RenderFrameProps props) override;
};

} // namespace playground::sdl
