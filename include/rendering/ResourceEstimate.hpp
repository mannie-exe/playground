#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace playground::rendering {
struct TextureStorageDesc {
  std::size_t width{}, height{}, depthOrLayers{1};
  unsigned levels{1}, samples{1};
  std::size_t blockWidth{1}, blockHeight{1}, blockBytes{4};
  bool volume{};
};

inline std::size_t checkedProduct(std::size_t a, std::size_t b) {
  if (b && a > std::numeric_limits<std::size_t>::max() / b)
    throw std::overflow_error("Resource estimate multiplication overflow");
  return a * b;
}

inline std::size_t checkedSum(std::size_t a, std::size_t b) {
  if (a > std::numeric_limits<std::size_t>::max() - b)
    throw std::overflow_error("Resource estimate addition overflow");
  return a + b;
}

// Nominal backing storage, excluding driver padding/metadata. Uses allocation
// extent, not the viewport. Each mip, layer and sample contributes once.
inline std::size_t estimateTextureStorage(TextureStorageDesc d) {
  if (!d.width || !d.height || !d.depthOrLayers || !d.levels || d.levels > 32 ||
      !d.samples || !d.blockWidth || !d.blockHeight || !d.blockBytes)
    throw std::invalid_argument("Invalid texture storage descriptor");
  std::size_t total{};
  for (unsigned level = 0; level < d.levels; ++level) {
    const auto blocksX = d.width / d.blockWidth + (d.width % d.blockWidth != 0);
    const auto blocksY =
        d.height / d.blockHeight + (d.height % d.blockHeight != 0);
    total = checkedSum(
        total, checkedProduct(checkedProduct(checkedProduct(blocksX, blocksY),
                                             d.depthOrLayers),
                              checkedProduct(d.blockBytes, d.samples)));
    d.width = std::max<std::size_t>(1, d.width / 2);
    d.height = std::max<std::size_t>(1, d.height / 2);
    if (d.volume)
      d.depthOrLayers = std::max<std::size_t>(1, d.depthOrLayers / 2);
  }
  return total;
}
} // namespace playground::rendering
