#include <stdexcept>

#include <platform/sdl/GPURenderBackend.hpp>
#include <platform/sdl/RenderBackendFactory.hpp>
#include <platform/sdl/SurfaceRenderBackend.hpp>

namespace playground::sdl {

rendering::RendererState
resolveRenderer(rendering::RendererPreferences requested,
                rendering::RendererRequirements needs) {
  auto candidates = availableGPURenderers();
  candidates.push_back(SurfaceRenderBackend::availableDescription());
  return rendering::selectRenderer(requested, needs, candidates).state;
}

std::unique_ptr<rendering::RenderBackend>
createRenderBackend(SDL_Window &window, const rendering::RendererState &state,
                    const rendering::RenderBackendProps &props) {
  props.validate();
  state.requested.validate();
  state.selected.validate();
  if (state.selected.backend == rendering::RendererKind::SDLGPU) {
    if (state.selected.capabilities !=
        rendering::RendererCapabilities{
            true, true, rendering::CompositionSpace::Linear, true})
      throw std::invalid_argument("GPU selection has invented capabilities");
    return std::make_unique<GPURenderBackend>(window, state.selected.driver,
                                              props);
  }
  const auto available = SurfaceRenderBackend::availableDescription();
  if (state.selected.backend != available.backend ||
      state.selected.driver != available.driver ||
      state.selected.capabilities != available.capabilities)
    throw std::invalid_argument(
        "Renderer selection is not available in this build");
  return std::make_unique<SurfaceRenderBackend>(window, props);
}

RenderBackendResult
createRenderBackend(SDL_Window &window,
                    rendering::RendererPreferences preferences,
                    rendering::RendererRequirements requirements,
                    const rendering::RenderBackendProps &props) {
  props.validate();
  auto candidates = availableGPURenderers();
  candidates.push_back(SurfaceRenderBackend::availableDescription());
  std::string failures;
  while (!candidates.empty()) {
    auto selection = [&] {
      try {
        return rendering::selectRenderer(preferences, requirements, candidates);
      } catch (const std::exception &error) {
        if (failures.empty())
          throw;
        throw std::runtime_error("Renderer creation failed: " + failures +
                                 error.what());
      }
    }();
    try {
      auto backend = createRenderBackend(window, selection.state, props);
      backend->prepare(requirements);
      if (!failures.empty())
        selection.state.fallbackReason = failures;
      return {std::move(backend), std::move(selection.state)};
    } catch (const std::exception &error) {
      failures +=
          std::string(rendering::toString(selection.state.selected.backend)) +
          "/" +
          std::string(rendering::toString(selection.state.selected.driver)) +
          ": " + error.what() + "; ";
      candidates.erase(candidates.begin() + selection.candidateIndex);
    }
  }
  throw std::runtime_error("Renderer creation failed: " + failures);
}

} // namespace playground::sdl
