#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include <rendering/Texture.hpp>
#include <support/PreparationBudget.hpp>

namespace playground::sdl {
// Shared default admission across concurrent decoders. Explicit budgets can
// replace it for an application/worker domain; no implicit waiting or threads.
PreparationBudget &texturePreparationBudget();
rendering::TextureHandle
decodeKTX2(std::span<const std::byte> bytes, rendering::TextureRole role,
           std::size_t maximumBytes = 256 * 1024 * 1024,
           PreparationBudget &budget = texturePreparationBudget());
rendering::TextureHandle
decodeTexture(std::span<const std::byte> bytes, std::string_view mime,
              rendering::TextureRole role,
              rendering::MipPolicy mip = rendering::MipPolicy::Generate,
              std::size_t maximumBytes = 256 * 1024 * 1024,
              PreparationBudget &budget = texturePreparationBudget());
rendering::TextureHandle
decodeHDR(std::span<const std::byte> bytes,
          std::size_t maximumBytes = 256 * 1024 * 1024,
          PreparationBudget &budget = texturePreparationBudget(),
          rendering::TextureFormat storage = rendering::TextureFormat::RGBA16F);
} // namespace playground::sdl
