#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include <rendering/GPUTiming.hpp>
#include <rendering/Submission.hpp>

struct SDL_GPUTimestampInterface;

namespace playground::sdl {

struct GPUTimestampProps {
  std::uint32_t capacity{128};
};

// Borrows its device. All calls, including destruction, belong to its owner
// thread. A ticket covers one command buffer, outside its render/copy passes.
class GPUTimestampRing {
  struct Impl;
  std::unique_ptr<Impl> _impl;

public:
  struct Ticket {
    std::uint32_t slot{};
    std::uint64_t generation{};
    bool operator==(const Ticket &) const = default;
  };

  explicit GPUTimestampRing(SDL_GPUDevice *, GPUTimestampProps = {});
  // Native extension injection also permits deterministic driver-contract
  // tests.
  explicit GPUTimestampRing(const SDL_GPUTimestampInterface &,
                            GPUTimestampProps = {});
  ~GPUTimestampRing();

  std::optional<Ticket> begin(SDL_GPUCommandBuffer *, std::string_view label);
  void end(SDL_GPUCommandBuffer *, Ticket);
  void submitted(Ticket, rendering::SubmissionId);
  void cancel(Ticket) noexcept;
  void abandon(Ticket) noexcept;
  std::vector<rendering::GPUTimingSample>
  poll(rendering::SubmissionId completedSubmission);
  bool supported() const noexcept;
  std::uint64_t dropped() const noexcept;

  GPUTimestampRing(const GPUTimestampRing &) = delete;
  GPUTimestampRing &operator=(const GPUTimestampRing &) = delete;
};

} // namespace playground::sdl
