#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include <platform/sdl/GPUTimestamps.hpp>
#include <rendering/AllocationLimits.hpp>
#include <rendering/ResourceDomain.hpp>
#include <rendering/Submission.hpp>
#include <support/SDLResource.hpp>

namespace playground::sdl {

struct GPUDeviceProps {
  SDL_GPUShaderFormat shaderFormats;
  bool debug{true};
  const char *driver{nullptr};
  rendering::AllocationLimits limits;
};

// Narrow native boundary for deterministic submission-failure verification.
// An override must consume the command buffer, just like SDL's submit call.
struct GPUCommandAPI {
  decltype(&SDL_SubmitGPUCommandBufferAndAcquireFence) submit{
      SDL_SubmitGPUCommandBufferAndAcquireFence};
};

// Native resources retain the device, but SDL initialization remains external.
class GPUDevice {
  struct Recording {
    std::vector<rendering::ResourceLease> uses;
    std::optional<GPUTimestampRing::Ticket> timestamp;
  };

  struct Pending {
    rendering::SubmissionId id{};
    SDL_GPUFence *fence{};
    std::vector<rendering::ResourceLease> uses;
    std::optional<std::chrono::steady_clock::time_point> submittedAt;
  };

  SDLResource<SDL_GPUDevice, SDL_DestroyGPUDevice> _device;
  rendering::AllocationLimits _limits;
  GPUCommandAPI _commands;
  rendering::ResourceDomainId _domain{rendering::acquireResourceDomain()};
  std::thread::id _owner{std::this_thread::get_id()};

  std::unordered_map<SDL_GPUCommandBuffer *, Recording> _recordings;
  std::deque<Pending> _pending;
  std::unordered_map<SDL_GPUTexture *, std::weak_ptr<rendering::ResourceUse>>
      _textures;
  rendering::SubmissionId _submitted{};
  rendering::SubmissionId _completed{};
  bool _valid{true};
  bool _profiling{};
  bool _timestampSupported{};
  std::unique_ptr<GPUTimestampRing> _timestamps;
  std::vector<rendering::GPUTimingSample> _timings;
  std::unordered_map<rendering::SubmissionId, double> _completionLatencies;
  std::uint64_t _collectionGeneration{};
  std::uint64_t _queryDropBaseline{};
  std::uint64_t _bufferDiscards{};

public:
  explicit GPUDevice(GPUDeviceProps props, GPUCommandAPI commands = {});
  ~GPUDevice();

  SDL_GPUDevice *get() const noexcept { return _device.get(); }

  const rendering::AllocationLimits &limits() const noexcept { return _limits; }

  rendering::ResourceDomainId resourceDomain() const noexcept {
    return _domain;
  }

  void checkOwnerThread() const;
  SDL_GPUCommandBuffer *acquireCommands(std::string_view label = "commands",
                                        rendering::GPUWorkContext context = {});
  void setTimingContext(SDL_GPUCommandBuffer *, rendering::GPUWorkContext);
  void recordUse(SDL_GPUCommandBuffer *, rendering::ResourceLease);
  void registerTexture(SDL_GPUTexture *, const rendering::ResourceLease &);
  void recordTexture(SDL_GPUCommandBuffer *, SDL_GPUTexture *);
  rendering::SubmissionId submit(SDL_GPUCommandBuffer *);
  void cancel(SDL_GPUCommandBuffer *) noexcept;
  void finishAbandonedPresentation(SDL_GPUCommandBuffer *) noexcept;
  void pollCompletions();

  rendering::SubmissionId completedSubmission() const noexcept {
    return _completed;
  }

  std::size_t pendingSubmissions() const noexcept { return _pending.size(); }

  void invalidate() noexcept;
  void setProfilingEnabled(bool enabled);
  bool supportsTimestamps() const noexcept;
  std::vector<rendering::GPUTimingSample> takeGPUTimings();
  rendering::GPUTimingCollection gpuTimingCollection() const;

  GPUDevice(const GPUDevice &) = delete;
  GPUDevice &operator=(const GPUDevice &) = delete;
};

using GPUDeviceHandle = std::shared_ptr<GPUDevice>;

} // namespace playground::sdl
