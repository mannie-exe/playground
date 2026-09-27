#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu_timestamps_playground.h>
#include <SDL3/SDL_properties.h>

#include <platform/sdl/GPUTimestamps.hpp>
#include <rendering/RenderFailure.hpp>

namespace playground::sdl {

struct GPUTimestampRing::Impl {
  enum class State { Free, Recording, Ended, Submitted, Abandoned };

  struct Slot {
    State state{};
    std::uint64_t generation{};
    rendering::SubmissionId submission{};
    SDL_GPUCommandBuffer *commands{};
    std::string label;
    rendering::GPUWorkContext context;
    std::uint64_t collectionGeneration{};
  };

  SDL_GPUTimestampInterface api{};
  void *pool{};
  std::thread::id owner{std::this_thread::get_id()};
  std::vector<Slot> slots;

  std::uint64_t sequence{};
  std::uint64_t dropped{};
  rendering::SubmissionId completed{};

  Impl(const SDL_GPUTimestampInterface *source, GPUTimestampProps props) {
    if (!props.capacity || props.capacity > 4096)
      throw std::invalid_argument("GPU timestamp capacity must be 1..4096");
    if (!source)
      return;
    if (source->version != SDL_PLAYGROUND_GPU_TIMESTAMPS_VERSION ||
        source->struct_size < sizeof(SDL_GPUTimestampInterface))
      throw std::invalid_argument(
          "Incompatible SDL timestamp extension version");
    api = *source;
    if (!api.valid_bits || api.valid_bits > 64 ||
        !std::isfinite(api.period_nanoseconds) || api.period_nanoseconds <= 0 ||
        !api.CreatePool || !api.Write || !api.Read || !api.ReleasePool)
      throw std::invalid_argument("Invalid SDL timestamp extension contract");
    slots.resize(props.capacity);
    pool = api.CreatePool(api.context, props.capacity);
    if (!pool)
      throw std::runtime_error(std::string{"Create timestamp pool: "} +
                               SDL_GetError());
  }

  ~Impl() {
    if (pool)
      api.ReleasePool(pool);
  }

  void checkOwner() const {
    if (owner != std::this_thread::get_id())
      throw std::logic_error("GPU timestamps require their owner thread");
  }

  Slot *find(Ticket ticket) noexcept {
    if (ticket.slot >= slots.size() || !ticket.generation)
      return nullptr;
    auto &slot = slots[ticket.slot];
    return slot.generation == ticket.generation ? &slot : nullptr;
  }
};

namespace {
const SDL_GPUTimestampInterface *extension(SDL_GPUDevice *device) {
  if (!device)
    throw std::invalid_argument("GPU timestamp device must not be null");
  return static_cast<const SDL_GPUTimestampInterface *>(SDL_GetPointerProperty(
      SDL_GetGPUDeviceProperties(device),
      SDL_PROP_GPU_DEVICE_PLAYGROUND_TIMESTAMPS_POINTER, nullptr));
}
} // namespace

GPUTimestampRing::GPUTimestampRing(SDL_GPUDevice *device,
                                   GPUTimestampProps props)
    : _impl{std::make_unique<Impl>(extension(device), props)} {}

GPUTimestampRing::GPUTimestampRing(const SDL_GPUTimestampInterface &api,
                                   GPUTimestampProps props)
    : _impl{std::make_unique<Impl>(&api, props)} {}

GPUTimestampRing::~GPUTimestampRing() = default;

std::optional<GPUTimestampRing::Ticket>
GPUTimestampRing::begin(SDL_GPUCommandBuffer *commands, std::string_view label,
                        rendering::GPUWorkContext context,
                        std::uint64_t collectionGeneration) {
  _impl->checkOwner();
  rendering::validateGPUTimingLabel(label);
  rendering::validateGPUWorkContext(context);
  if (!supported())
    return std::nullopt;
  if (!commands)
    throw std::invalid_argument("Timestamp command buffer must not be null");
  for (const auto &slot : _impl->slots)
    if (slot.commands == commands && (slot.state == Impl::State::Recording ||
                                      slot.state == Impl::State::Ended))
      throw std::logic_error("Command buffer already has a timestamp scope");
  for (std::uint32_t index{}; index < _impl->slots.size(); ++index) {
    auto &slot = _impl->slots[index];
    if (slot.state != Impl::State::Free)
      continue;
    if (_impl->sequence == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("GPU timestamp sequence exhausted");
    std::string savedLabel{label};
    if (!_impl->api.Write(_impl->pool, commands, index, false))
      throw rendering::RenderFailure(std::string{"Begin GPU timestamp: "} +
                                         SDL_GetError(),
                                     rendering::RenderOperation::Record);
    slot.label = std::move(savedLabel);
    slot.context = context;
    slot.collectionGeneration = collectionGeneration;
    slot.generation = ++_impl->sequence;
    slot.commands = commands;
    slot.state = Impl::State::Recording;
    return Ticket{index, slot.generation};
  }
  if (_impl->dropped != std::numeric_limits<std::uint64_t>::max())
    ++_impl->dropped;
  return std::nullopt;
}

void GPUTimestampRing::setContext(Ticket ticket,
                                  rendering::GPUWorkContext context) {
  _impl->checkOwner();
  rendering::validateGPUWorkContext(context);
  auto *slot = _impl->find(ticket);
  if (!slot || slot->state != Impl::State::Recording)
    throw std::logic_error("Timestamp metadata requires a recording ticket");
  slot->context = context;
}

void GPUTimestampRing::end(SDL_GPUCommandBuffer *commands, Ticket ticket) {
  _impl->checkOwner();
  auto *slot = _impl->find(ticket);
  if (!slot || slot->state != Impl::State::Recording ||
      slot->commands != commands)
    throw std::logic_error(
        "Timestamp end requires its recording command buffer");
  if (!_impl->api.Write(_impl->pool, commands, ticket.slot, true))
    throw rendering::RenderFailure(std::string{"End GPU timestamp: "} +
                                       SDL_GetError(),
                                   rendering::RenderOperation::Record);
  slot->state = Impl::State::Ended;
}

void GPUTimestampRing::submitted(Ticket ticket,
                                 rendering::SubmissionId submission) {
  _impl->checkOwner();
  auto *slot = _impl->find(ticket);
  if (!slot || slot->state != Impl::State::Ended || !submission ||
      submission <= _impl->completed)
    throw std::logic_error(
        "Timestamp submission must follow end and precede completion");
  slot->submission = submission;
  slot->commands = nullptr;
  slot->state = Impl::State::Submitted;
}

void GPUTimestampRing::cancel(Ticket ticket) noexcept {
  if (_impl->owner != std::this_thread::get_id())
    return;
  auto *slot = _impl->find(ticket);
  if (slot && (slot->state == Impl::State::Recording ||
               slot->state == Impl::State::Ended)) {
    slot->state = Impl::State::Free;
    slot->commands = nullptr;
    slot->label.clear();
  }
}

void GPUTimestampRing::abandon(Ticket ticket) noexcept {
  if (_impl->owner != std::this_thread::get_id())
    return;
  if (auto *slot = _impl->find(ticket);
      slot && slot->state != Impl::State::Free) {
    slot->state = Impl::State::Abandoned;
    slot->commands = nullptr;
    slot->label.clear();
  }
}

std::vector<rendering::GPUTimingSample>
GPUTimestampRing::poll(rendering::SubmissionId completedSubmission) {
  _impl->checkOwner();
  if (completedSubmission < _impl->completed)
    throw std::logic_error("GPU timestamp completion cannot move backwards");
  _impl->completed = completedSubmission;
  std::vector<rendering::GPUTimingSample> samples;
  samples.reserve(_impl->slots.size());
  std::vector<std::uint32_t> ready;
  ready.reserve(_impl->slots.size());
  for (std::uint32_t index{}; index < _impl->slots.size(); ++index) {
    auto &slot = _impl->slots[index];
    if (slot.state != Impl::State::Submitted ||
        slot.submission > completedSubmission)
      continue;
    Uint64 begin{}, end{};
    const auto status = _impl->api.Read(_impl->pool, index, &begin, &end);
    if (status == SDL_GPU_TIMESTAMP_NOT_READY)
      continue;
    if (status != SDL_GPU_TIMESTAMP_READY) {
      slot.state = Impl::State::Abandoned;
      throw rendering::RenderFailure(std::string{"Read GPU timestamp: "} +
                                         SDL_GetError(),
                                     rendering::RenderOperation::Query);
    }
    const auto mask = _impl->api.valid_bits == 64
                          ? std::numeric_limits<std::uint64_t>::max()
                          : (std::uint64_t{1} << _impl->api.valid_bits) - 1;
    const auto ticks = (end - begin) & mask;
    const double milliseconds = static_cast<double>(ticks) *
                                _impl->api.period_nanoseconds / 1'000'000.0;
    samples.push_back({slot.submission,
                       slot.label,
                       milliseconds,
                       {},
                       {},
                       slot.context,
                       slot.collectionGeneration});
    ready.push_back(index);
  }
  // Slot reuse changes physical order; publish this batch in submission order.
  std::ranges::sort(samples, {}, &rendering::GPUTimingSample::sequence);
  // Commit only after every read/allocation succeeded; exceptions preserve
  // already-ready samples for the next poll instead of silently consuming them.
  for (const auto index : ready) {
    _impl->slots[index].label.clear();
    _impl->slots[index].state = Impl::State::Free;
  }
  return samples;
}

bool GPUTimestampRing::supported() const noexcept {
  return _impl->pool != nullptr;
}

std::uint64_t GPUTimestampRing::dropped() const noexcept {
  return _impl->dropped;
}

std::uint32_t
GPUTimestampRing::pending(std::uint64_t generation) const noexcept {
  return static_cast<std::uint32_t>(
      std::ranges::count_if(_impl->slots, [generation](const auto &slot) {
        return slot.collectionGeneration == generation &&
               (slot.state == Impl::State::Recording ||
                slot.state == Impl::State::Ended ||
                slot.state == Impl::State::Submitted);
      }));
}

} // namespace playground::sdl
