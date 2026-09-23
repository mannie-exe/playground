#pragma once

#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <support/SDLResource.hpp>

#include <rendering/RenderBackend.hpp>

namespace playground::sdl {

// Borrows a window; acquires its current surface anew for each frame.
// UI-thread-only. The window must not be resized while a frame is alive.
class SurfaceRenderBackend final : public rendering::RenderBackend {
  SDL_Window &_window;

  bool _frameActive{};
  SDLResource<SDL_Surface, SDL_DestroySurface> _scaledTarget;

public:
  explicit SurfaceRenderBackend(SDL_Window &window) : _window{window} {}

  math::Vec2i drawableSize() const override;
  std::unique_ptr<rendering::RenderFrame>
  beginFrame(rendering::RenderFrameProps props) override;
};

} // namespace playground::sdl
