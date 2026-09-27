#pragma once

#include <stop_token>

#include <scene/SceneRenderer.hpp>

namespace playground::scene {
struct EnvironmentProps {
  unsigned diffuseWidth{32}, specularWidth{128}, brdfSize{64}, samples{128};
  void validate() const;
};

struct PreparedEnvironment {
  rendering::TextureHandle diffuse, specular, brdf;
};

// CPU-only, deterministic, cancelable. Environment latitude-longitude has +Y at
// the top and +Z at U=.5. Outputs are linear and immutable.
PreparedEnvironment prepareEnvironment(const rendering::Texture &source,
                                       EnvironmentProps props = {},
                                       std::stop_token stop = {});
} // namespace playground::scene
