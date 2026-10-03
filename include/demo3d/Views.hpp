#pragma once

#include <scene/Controllers.hpp>

namespace playground::demo3d {
// Authored street view shared by interactive demos and explicit captures.
// Preset selection is caller policy, never inferred from a model filename.
inline constexpr scene::FreeCameraProps bistroView{
    .position = {24.82285f, 3.16055f, 61.64814f},
    .yaw = -2.81696f,
    .pitch = -.05827f,
    .unitsPerSecond = 5};
} // namespace playground::demo3d
