#pragma once

#include <cstddef>
#include <string_view>

#include <math/Color.hpp>

namespace playground::config {
inline constexpr std::string_view defaultWindowTitle{"playground"};
inline constexpr playground::math::ColorRGBA8 defaultClearColor{50, 50, 50,
                                                                255};

inline constexpr std::size_t maxCachedFonts{8};
inline constexpr std::size_t maxCachedImages{64};
inline constexpr std::size_t maxCachedVectors{32};
inline constexpr std::size_t maxCachedSurfaceBytes{64 * 1024 * 1024};
} // namespace playground::config
