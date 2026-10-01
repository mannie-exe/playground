#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>

#include <math/Geometry2D.hpp>

namespace playground::rendering {

// Application safeguards, not queried free VRAM or guaranteed hardware limits.
struct AllocationLimits {
  std::uint32_t maxTextureDimension{16384};
  std::size_t maxTargetBytes{256 * 1024 * 1024};
  std::size_t maxUploadBytes{256 * 1024 * 1024};
  std::size_t maxResidentBytes{128 * 1024 * 1024};
  std::size_t maxMeshResidentBytes{128 * 1024 * 1024};
  std::size_t maxMaterialResidentBytes{256 * 1024 * 1024};
  std::size_t maxMaterialTextureBytes{128 * 1024 * 1024};
  std::size_t maxTargetPoolBytes{512 * 1024 * 1024};
  std::size_t maxLivePoolBytes{1024 * 1024 * 1024};
  std::size_t maxStreamBytes{16 * 1024 * 1024};
  std::size_t maxInFlightSubmissions{256};
  std::uint32_t maxTimestampScopes{128};
  std::size_t maxSoftwareTargetPixels{32 * 1024 * 1024};
  std::uint64_t targetRetentionSubmissions{240};

  void validate() const {
    if (!maxTextureDimension || !maxTargetBytes || !maxUploadBytes ||
        !maxStreamBytes || !maxInFlightSubmissions ||
        !maxSoftwareTargetPixels || !maxLivePoolBytes ||
        !maxMaterialTextureBytes)
      throw std::invalid_argument(
          "Renderer allocation limits must be positive");
    if (maxUploadBytes > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(
          "Upload policy exceeds native 32-bit capacity");
    if (maxStreamBytes > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(
          "Streaming capacity exceeds native 32-bit capacity");
    if (!maxTimestampScopes || maxTimestampScopes > 4096)
      throw std::invalid_argument("Timestamp scope capacity must be 1..4096");
  }

  static std::size_t textureBytes(
      math::Vec2i size, std::size_t bytesPerPixel,
      std::string_view context = "Texture") {
    if (!math::hasArea(size) || !bytesPerPixel)
      throw std::invalid_argument(std::format(
          "{}: dimensions and pixel size must be positive ({}x{}, {} bytes/pixel)",
          context, size.x, size.y, bytesPerPixel));
    const auto width = static_cast<std::size_t>(size.x);
    const auto height = static_cast<std::size_t>(size.y);
    if (width >
        std::numeric_limits<std::size_t>::max() / height / bytesPerPixel)
      throw std::overflow_error(std::format(
          "{}: {}x{} at {} bytes/pixel exceeds addressable storage", context,
          size.x, size.y, bytesPerPixel));
    return width * height * bytesPerPixel;
  }

  std::size_t validateTarget(math::Vec2i size,
                             std::size_t bytesPerPixel,
                             std::string_view context = "Render target") const {
    validate();
    const auto bytes = textureBytes(size, bytesPerPixel, context);
    const bool dimensionsExceeded =
        static_cast<std::uint32_t>(size.x) > maxTextureDimension ||
        static_cast<std::uint32_t>(size.y) > maxTextureDimension;
    if (dimensionsExceeded || bytes > maxTargetBytes)
      throw std::length_error(std::format(
          "{} exceeds {} limit: {}x{} at {} bytes/pixel requires {} bytes; "
          "limits: {} pixels/axis, {} bytes/target",
          context, dimensionsExceeded ? "dimension" : "byte", size.x, size.y,
          bytesPerPixel, bytes, maxTextureDimension, maxTargetBytes));
    return bytes;
  }

  void validateUpload(std::size_t bytes,
                      std::string_view context = "Upload") const {
    validate();
    if (!bytes || bytes > maxUploadBytes)
      throw std::length_error(std::format(
          "{} violates upload byte limit: requested {} bytes; allowed 1..{} bytes",
          context, bytes, maxUploadBytes));
  }

  bool operator==(const AllocationLimits &) const = default;
};

} // namespace playground::rendering
