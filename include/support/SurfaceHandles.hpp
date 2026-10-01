#pragma once

#include <memory>

#include <SDL3/SDL_surface.h>

#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceLedger.hpp>

struct SurfaceHandleDeleter {
  void operator()(SDL_Surface *surface) const {
    if (surface)
      SDL_DestroySurface(surface);
  }
};

using SurfaceHandle = std::shared_ptr<SDL_Surface>;

inline SurfaceHandle createManagedSurface(
    int width, int height,
    const std::shared_ptr<playground::rendering::ResourceLedger> &ledger =
        playground::rendering::defaultResourceLedger()) {
  using namespace playground::rendering;
  auto allocation = ledger->reserve(
      MemoryClass::CPU, ResourceKind::Surface,
      AllocationLimits::textureBytes({width, height}, 4), "Software surface");
  auto *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
  if (!surface)
    throw ResourceAllocationFailure(
        std::format("Software surface allocation failed: {}", SDL_GetError()));
  allocation->setState(AllocationState::Owned);
  return {surface, [allocation = std::move(allocation)](SDL_Surface *value) {
            SDL_DestroySurface(value);
          }};
}

// Decoders own their private allocation phase. Charge adopted pixels by pitch,
// preserving the charge with the shared surface rather than each image wrapper.
inline SurfaceHandle adoptManagedSurface(SDL_Surface *surface) {
  SurfaceHandle owner{surface, SurfaceHandleDeleter{}};
  if (!surface)
    throw std::runtime_error(SDL_GetError());
  using namespace playground::rendering;
  auto allocation = defaultResourceLedger()->reserve(
      MemoryClass::CPU, ResourceKind::Asset,
      AllocationLimits::textureBytes({surface->pitch, surface->h}, 1),
      "Decoded surface");
  allocation->setState(AllocationState::Owned);
  return {surface, [owner = std::move(owner),
                    allocation = std::move(allocation)](SDL_Surface *) mutable {
            owner.reset();
            allocation.reset();
          }};
}
