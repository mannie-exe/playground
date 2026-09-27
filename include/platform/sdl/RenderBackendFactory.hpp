#pragma once

#include <memory>

#include <SDL3/SDL_video.h>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderBackendProps.hpp>

namespace playground::sdl {

// Selection does not acquire a window surface or create a GPU device.
// Only implementations actually available in this build may be returned.
rendering::RendererState resolveRenderer(rendering::RendererPreferences,
                                         rendering::RendererRequirements);
std::unique_ptr<rendering::RenderBackend>
createRenderBackend(SDL_Window &, const rendering::RendererState &,
                    const rendering::RenderBackendProps & = {});

struct RenderBackendResult {
  std::unique_ptr<rendering::RenderBackend> backend;
  rendering::RendererState state;
};

// Attempts compatible candidates; creation failures may fall back only within
// the requested policy. Never relaxes the application's requirements.
RenderBackendResult
createRenderBackend(SDL_Window &, rendering::RendererPreferences,
                    rendering::RendererRequirements,
                    const rendering::RenderBackendProps & = {});

} // namespace playground::sdl
