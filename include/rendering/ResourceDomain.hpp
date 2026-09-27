#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace playground::rendering {

// Compatibility identity, not an address, resource owner, or completion fence.
// Zero is unspecified; one is the shared portable CPU-image domain.
struct ResourceDomainId {
  std::uint64_t value{};

  static constexpr ResourceDomainId cpu() noexcept { return {1}; }

  explicit constexpr operator bool() const noexcept { return value != 0; }

  constexpr bool operator==(const ResourceDomainId &) const = default;
};

inline ResourceDomainId acquireResourceDomain() {
  static std::atomic<std::uint64_t> next{2};
  auto candidate = next.load(std::memory_order_relaxed);
  for (;;) {
    if (candidate == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("Renderer resource domains exhausted");
    if (next.compare_exchange_weak(candidate, candidate + 1,
                                   std::memory_order_relaxed))
      return {candidate};
  }
}

} // namespace playground::rendering
