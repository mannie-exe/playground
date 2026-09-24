#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace playground::rendering {

enum class RendererKind { Software, SDLGPU };
enum class RendererChoice { Auto, Software, SDLGPU };
enum class GPUDriver { Auto, Vulkan };
enum class CompositionSpace { EncodedSRGB, Linear };

struct RendererRequirements {
  bool paint2D{true};
  bool scene3D{};
  bool linearComposition{};
  bool operator==(const RendererRequirements &) const = default;
};

struct RendererPreferences {
  RendererChoice backend{RendererChoice::Auto};
  GPUDriver driver{GPUDriver::Auto};
  bool allowFallback{true};
  void validate() const;
  bool operator==(const RendererPreferences &) const = default;
};

struct RendererCapabilities {
  bool paint2D{};
  bool scene3D{};
  CompositionSpace composition{CompositionSpace::EncodedSRGB};
  bool supports(RendererRequirements requirements) const;
  bool operator==(const RendererCapabilities &) const = default;
};

// A candidate represents an available implementation, not a speculative driver.
// Auto is only a preference; an actual GPU candidate names a concrete driver.
struct RendererCandidate {
  RendererKind backend{RendererKind::Software};
  GPUDriver driver{GPUDriver::Auto};
  RendererCapabilities capabilities;
  void validate() const;
};

struct RendererState {
  RendererPreferences requested;
  RendererCandidate selected;
  std::string fallbackReason;
  bool isFallback() const noexcept { return !fallbackReason.empty(); }
};

struct RendererSelection {
  std::size_t candidateIndex;
  RendererState state;
};

// Pure negotiation: no window, device allocation, IO or implicit fallback.
// Auto prefers GPU, then software. Equal-priority candidates retain input
// order.
RendererSelection selectRenderer(RendererPreferences preferences,
                                 RendererRequirements requirements,
                                 std::span<const RendererCandidate> candidates);

std::string_view toString(RendererKind);
std::string_view toString(RendererChoice);
std::string_view toString(GPUDriver);

} // namespace playground::rendering
