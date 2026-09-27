#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include <rendering/RendererTypes.hpp>

namespace playground::rendering {

void RendererPreferences::validate() const {
  (void)toString(backend);
  (void)toString(driver);
}

bool RendererCapabilities::supports(RendererRequirements needs) const {
  if (composition != CompositionSpace::EncodedSRGB &&
      composition != CompositionSpace::Linear)
    throw std::invalid_argument("Unknown composition space");
  return (!needs.paint2D || paint2D) && (!needs.scene3D || scene3D) &&
         (!needs.linearComposition ||
          composition == CompositionSpace::Linear) &&
         (!needs.metallicRoughness || (metallicRoughness && scene3D));
}

void RendererCandidate::validate() const {
  (void)toString(backend);
  (void)toString(driver);
  (void)capabilities.supports({});
  if ((backend == RendererKind::Software && driver != GPUDriver::Auto) ||
      (backend == RendererKind::SDLGPU && driver == GPUDriver::Auto))
    throw std::invalid_argument(
        "Renderer candidate has an invalid native driver");
}

RendererSelection
selectRenderer(RendererPreferences preferences,
               RendererRequirements requirements,
               std::span<const RendererCandidate> candidates) {
  preferences.validate();
  std::optional<std::size_t> best;
  int bestRank = std::numeric_limits<int>::max();
  bool selectedFallback{};
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    const auto &candidate = candidates[i];
    candidate.validate();
    if (!candidate.capabilities.supports(requirements))
      continue;
    const bool gpu = candidate.backend == RendererKind::SDLGPU;
    const bool backendMatches =
        preferences.backend == RendererChoice::Auto ||
        (preferences.backend == RendererChoice::SDLGPU && gpu) ||
        (preferences.backend == RendererChoice::Software && !gpu);
    // The GPU driver preference is dormant under explicit software selection.
    const bool driverMatches =
        preferences.backend == RendererChoice::Software ||
        preferences.driver == GPUDriver::Auto ||
        (gpu && preferences.driver == candidate.driver);
    const bool fallback = !backendMatches || !driverMatches;
    if (fallback && !preferences.allowFallback)
      continue;
    const int rank = (fallback ? 10 : 0) + (backendMatches ? 0 : 4) +
                     (driverMatches ? 0 : 2) + (gpu ? 0 : 1);
    if (rank < bestRank) {
      best = i;
      bestRank = rank;
      selectedFallback = fallback;
    }
  }
  if (!best)
    throw std::runtime_error("No available renderer satisfies app requirements "
                             "and renderer preferences");
  RendererState state{preferences, candidates[*best], {}};
  if (selectedFallback)
    state.fallbackReason = "Requested backend/driver is unavailable or "
                           "incompatible with app requirements";
  return {*best, std::move(state)};
}

std::string_view toString(RendererKind kind) {
  switch (kind) {
  case RendererKind::Software:
    return "software";
  case RendererKind::SDLGPU:
    return "sdl-gpu";
  }
  throw std::invalid_argument("Unknown renderer kind");
}

std::string_view toString(RendererChoice choice) {
  switch (choice) {
  case RendererChoice::Auto:
    return "auto";
  case RendererChoice::Software:
    return "software";
  case RendererChoice::SDLGPU:
    return "sdl-gpu";
  }
  throw std::invalid_argument("Unknown renderer choice");
}

std::string_view toString(GPUDriver driver) {
  switch (driver) {
  case GPUDriver::Auto:
    return "auto";
  case GPUDriver::Vulkan:
    return "vulkan";
  }
  throw std::invalid_argument("Unknown GPU driver");
}

} // namespace playground::rendering
