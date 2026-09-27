#include <memory>
#include <stdexcept>

#include <runtime/DeferredMutations.hpp>
#include <support/Test.hpp>

using namespace playground::runtime;
using playground::test::require;

int main() {
  return playground::test::run([] {
    CompletionQueue queue;
    ActivationLifetime lifetime;
    require(!lifetime.token().isActive(), "inactive by default");
    lifetime.activate();
    auto first = lifetime.token();
    ActivationSink sink{queue.sink(), first};
    int calls{};
    require(sink.post([&] { ++calls; }), "active owner accepted");
    lifetime.deactivate();
    lifetime.activate();
    queue.drain();
    require(calls == 0 && !first.isActive(),
            "reactivation cannot revive stale callback");
    require(!sink.post([&] { ++calls; }), "old endpoint rejects admission");
    require(queue.sink().post(lifetime.token(), [&] { ++calls; }),
            "current token admitted");
    queue.drain();
    require(calls == 1, "live owner receives completion");
    queue.sink().post(lifetime.token(),
                      [] { throw std::runtime_error("expected"); });
    queue.sink().post(lifetime.token(), [&] { ++calls; });
    try {
      queue.drain();
    } catch (const std::runtime_error &) {
    }
    require(queue.pending() == 1, "throw preserves untouched tail");
    queue.drain();
    require(calls == 2, "tail retries next boundary");
    DeferredMutations mutations{{.maxPending = 1, .maxPerDrain = 1}};
    auto object = std::make_unique<ActivationLifetime>();
    object->activate();
    auto token = object->token();
    require(mutations.remove(*object, [&] { object.reset(); }),
            "removal admitted");
    require(object && !token.isActive(),
            "pending removal inactive before destruction");
    require(!mutations.defer([] {}), "bounded mutation admission");
    mutations.flush();
    require(!object, "destruction at safe boundary");
    ActivationToken destroyed;
    {
      ActivationLifetime owner;
      owner.activate();
      destroyed = owner.token();
    }
    require(!destroyed.isActive(), "destruction invalidates token");
  });
}
