#pragma once

#include <format>
#include <string_view>

#include <rendering/PaintImage.hpp>
#include <rendering/RendererTypes.hpp>

namespace playground::rendering {
constexpr std::string_view toString(Sampling value) {
  switch (value) {
  case Sampling::Nearest:
    return "Nearest";
  case Sampling::Linear:
    return "Linear";
  }
  return "Unknown";
}
} // namespace playground::rendering

template <>
struct std::formatter<playground::rendering::Sampling>
    : std::formatter<std::string_view> {
  auto format(playground::rendering::Sampling value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::rendering::toString(value), context);
  }
};
template <>
struct std::formatter<playground::rendering::RendererKind>
    : std::formatter<std::string_view> {
  auto format(playground::rendering::RendererKind value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::rendering::toString(value), context);
  }
};
template <>
struct std::formatter<playground::rendering::RendererChoice>
    : std::formatter<std::string_view> {
  auto format(playground::rendering::RendererChoice value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::rendering::toString(value), context);
  }
};
template <>
struct std::formatter<playground::rendering::GPUDriver>
    : std::formatter<std::string_view> {
  auto format(playground::rendering::GPUDriver value,
              std::format_context &context) const {
    return std::formatter<std::string_view>::format(
        playground::rendering::toString(value), context);
  }
};
