#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

#include <SDL3/SDL_gpu_timestamps_playground.h>

#include <platform/sdl/GPUTimestamps.hpp>
#include <rendering/RenderFailure.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
struct Driver {
  struct Result {
    SDL_GPUTimestampReadStatus status{SDL_GPU_TIMESTAMP_READY};
    Uint64 begin{10};
    Uint64 end{50};
  };
  std::array<Result, 4> results;
  unsigned writes{};
  unsigned reads{};
  unsigned releases{};
  bool failWrite{};
  bool failCreate{};

  SDL_GPUTimestampInterface api() {
    return {SDL_PLAYGROUND_GPU_TIMESTAMPS_VERSION,
            sizeof(SDL_GPUTimestampInterface),
            64,
            1000.0f,
            this,
            [](void *context, Uint32) -> void * {
              auto *self = static_cast<Driver *>(context);
              return self->failCreate ? nullptr : self;
            },
            [](void *pool, SDL_GPUCommandBuffer *, Uint32, bool) -> bool {
              auto &self = *static_cast<Driver *>(pool);
              ++self.writes;
              return !self.failWrite;
            },
            [](void *pool, Uint32 index, Uint64 *begin,
               Uint64 *end) -> SDL_GPUTimestampReadStatus {
              auto &self = *static_cast<Driver *>(pool);
              ++self.reads;
              const auto &result = self.results.at(index);
              *begin = result.begin;
              *end = result.end;
              return result.status;
            },
            [](void *pool) { ++static_cast<Driver *>(pool)->releases; }};
  }
};

template <typename Exception = std::logic_error, typename Function>
void rejects(Function &&function) {
  bool caught{};
  try {
    function();
  } catch (const Exception &) {
    caught = true;
  }
  test::require(caught, "invalid timestamp operation must throw");
}

void lifetimeAndCompletion() {
  Driver driver;
  // Opaque command identities are never dereferenced by the injected driver.
  int first{}, second{};
  auto *a = reinterpret_cast<SDL_GPUCommandBuffer *>(&first);
  auto *b = reinterpret_cast<SDL_GPUCommandBuffer *>(&second);
  {
    sdl::GPUTimestampRing ring{driver.api(), {1}};
    test::require(ring.supported(), "valid native table provides timing");
    auto ticket = *ring.begin(a, "frame");
    rejects([&] { ring.begin(a, "duplicate"); });
    rejects([&] { ring.end(b, ticket); });
    rejects([&] { ring.submitted(ticket, 1); });
    test::require(!ring.begin(b, "full") && ring.dropped() == 1,
                  "full pool drops telemetry without blocking rendering");
    ring.end(a, ticket);
    ring.submitted(ticket, 1);
    ring.cancel(ticket);
    test::require(ring.poll(0).empty() && driver.reads == 0,
                  "no native read before submission completion");
    driver.results[0].status = SDL_GPU_TIMESTAMP_NOT_READY;
    test::require(ring.poll(1).empty() && !ring.begin(b, "still full"),
                  "unavailable result retains slot even after completion");
    driver.results[0].status = SDL_GPU_TIMESTAMP_READY;
    const auto samples = ring.poll(1);
    test::require(samples.size() == 1 && samples[0].label == "frame" &&
                      std::abs(samples[0].milliseconds - 0.04) < 1e-12,
                  "timestamp ticks convert through native period");
    auto reused = *ring.begin(b, "reuse");
    test::require(reused.generation != ticket.generation,
                  "reused slots have fresh generations");
    ring.cancel(ticket);
    rejects([&] { ring.end(b, ticket); });
    ring.cancel(reused);
    auto canceled = *ring.begin(a, "cancel");
    ring.cancel(canceled);
    auto abandoned = *ring.begin(a, "ambiguous submit");
    ring.abandon(abandoned);
    test::require(!ring.begin(b, "quarantined"),
                  "ambiguous native failure cannot recycle GPU query memory");
    rejects([&] { ring.poll(0); });
  }
  test::require(driver.releases == 1, "native client closes exactly once");
}

void wrapAndFailures() {
  Driver driver;
  int identity{};
  auto *commands = reinterpret_cast<SDL_GPUCommandBuffer *>(&identity);
  auto api = driver.api();
  api.valid_bits = 8;
  driver.results[0] = {SDL_GPU_TIMESTAMP_READY, 250, 5};
  sdl::GPUTimestampRing ring{api, {2}};
  auto ticket = *ring.begin(commands, "wrap");
  ring.end(commands, ticket);
  ring.submitted(ticket, 1);
  test::require(std::abs(ring.poll(1)[0].milliseconds - 0.011) < 1e-12,
                "timestamp arithmetic masks hardware counter wrap");
  ticket = *ring.begin(commands, "failed write");
  driver.failWrite = true;
  rejects<rendering::RenderFailure>([&] { ring.end(commands, ticket); });
  ring.cancel(ticket);
  driver.failWrite = false;
  ticket = *ring.begin(commands, "lost");
  ring.end(commands, ticket);
  ring.submitted(ticket, 2);
  driver.results[ticket.slot].status = SDL_GPU_TIMESTAMP_DEVICE_LOST;
  rejects<rendering::RenderFailure>([&] { ring.poll(2); });
  test::require(ring.poll(2).empty(),
                "failed query is quarantined, not reread");

  bool rejectedOwner{};
  std::thread other([&] {
    try {
      ring.poll(2);
    } catch (const std::logic_error &) {
      rejectedOwner = true;
    }
  });
  other.join();
  test::require(rejectedOwner, "native timestamp calls stay owner-thread");
}

void malformedContracts() {
  Driver driver;
  auto api = driver.api();
  rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api, {0}}; });
  rejects<std::invalid_argument>(
      [&] { sdl::GPUTimestampRing ring{api, {4097}}; });
  for (const Uint32 bits : {0u, 65u}) {
    api.valid_bits = bits;
    rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api}; });
  }
  api = driver.api();
  api.period_nanoseconds = std::numeric_limits<float>::quiet_NaN();
  rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api}; });
  api = driver.api();
  api.version = 900;
  rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api}; });
  api = driver.api();
  api.Read = nullptr;
  rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api}; });
  api = driver.api();
  --api.struct_size;
  rejects<std::invalid_argument>([&] { sdl::GPUTimestampRing ring{api}; });
  api = driver.api();
  driver.failCreate = true;
  rejects<std::runtime_error>([&] { sdl::GPUTimestampRing ring{api}; });
  test::require(driver.releases == 0,
                "failed construction owns no native pool");
}
void preserveReadySamples() {
  Driver driver;
  int first{}, second{};
  auto *a = reinterpret_cast<SDL_GPUCommandBuffer *>(&first);
  auto *b = reinterpret_cast<SDL_GPUCommandBuffer *>(&second);
  sdl::GPUTimestampRing ring{driver.api(), {2}};
  const auto one = *ring.begin(a, "preserved");
  ring.end(a, one);
  ring.submitted(one, 10);
  const auto two = *ring.begin(b, "query failure");
  ring.end(b, two);
  ring.submitted(two, 11);
  driver.results[one.slot] = {SDL_GPU_TIMESTAMP_READY,
                              std::numeric_limits<Uint64>::max() - 3, 2};
  driver.results[two.slot].status = SDL_GPU_TIMESTAMP_ERROR;
  rejects<rendering::RenderFailure>([&] { ring.poll(11); });
  const auto samples = ring.poll(11);
  test::require(
      samples.size() == 1 && samples[0].sequence == 10 &&
          samples[0].label == "preserved" &&
          std::abs(samples[0].milliseconds - 0.006) < 1e-12,
      "failed later read preserves prior ready sample and 64-bit wrap");
}

void labelsAndRecycledOrder() {
  Driver driver;
  int first{}, second{};
  auto *a = reinterpret_cast<SDL_GPUCommandBuffer *>(&first);
  auto *b = reinterpret_cast<SDL_GPUCommandBuffer *>(&second);
  sdl::GPUTimestampRing ring{driver.api(), {2}};
  rejects<std::invalid_argument>([&] { ring.begin(a, ""); });
  rejects<std::invalid_argument>([&] {
    ring.begin(a, std::string(rendering::maximumGPUTimingLabelBytes + 1, 'x'));
  });
  test::require(driver.writes == 0,
                "invalid labels fail before recording native commands");
  const auto one =
      *ring.begin(a, std::string(rendering::maximumGPUTimingLabelBytes, 'x'));
  ring.end(a, one);
  ring.submitted(one, 1);
  const auto two = *ring.begin(b, "second");
  ring.end(b, two);
  ring.submitted(two, 2);
  test::require(ring.poll(1).size() == 1, "first slot becomes reusable");
  const auto three = *ring.begin(a, "third");
  test::require(three.slot == one.slot, "lower slot is recycled");
  ring.end(a, three);
  ring.submitted(three, 3);
  const auto ready = ring.poll(3);
  test::require(ready.size() == 2 && ready[0].sequence == 2 &&
                    ready[1].sequence == 3,
                "ready batches follow submission order, not slot order");
}
} // namespace

int main() {
  return test::run([] {
    lifetimeAndCompletion();
    wrapAndFailures();
    malformedContracts();
    preserveReadySamples();
    labelsAndRecycledOrder();
  });
}
