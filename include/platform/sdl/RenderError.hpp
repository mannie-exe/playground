#pragma once

#include <string_view>

#include <rendering/RenderFailure.hpp>
#include <support/SDLError.hpp>

namespace playground::sdl {

// Use only after a failed native frame operation. Bad props, unsupported
// capabilities and allocation-policy failures must keep their original types.
[[noreturn]] inline void
throwRenderError(std::string_view context,
                 rendering::RenderOperation operation) {
  throw rendering::RenderFailure{buildSDLErrorMessage(context), operation};
}

} // namespace playground::sdl
