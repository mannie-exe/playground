#pragma once

#include <ui/Theme.hpp>

namespace playground::sdl {
// Owner-thread snapshot. Unsupported/unavailable observations remain absent.
ui::SystemAppearance systemAppearance();
} // namespace playground::sdl
