#pragma once

#include <scene/Controllers.hpp>

namespace playground::demo3d {
// Authored street view shared by interactive demos and explicit captures.
// Preset selection is caller policy, never inferred from a model filename.
inline scene::FreeCameraProps bistroView(world::SpaceId space,
                                         std::uint64_t epoch) {
  return {.position = {space, {24.82285, 3.16055, 61.64814}},
          .epoch = epoch,
          .yaw = -2.81696f,
          .pitch = -.05827f,
          .unitsPerSecond = 5};
}
} // namespace playground::demo3d
