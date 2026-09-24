#include <atomic>
#include <thread>
#include <vector>

#include <runtime/CompletionQueue.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    test::rejects([] { runtime::CompletionQueue queue{{0, 1}}; },
                  "invalid capacity");
    runtime::CompletionQueue queue{{3, 2}};
    auto sink = queue.sink();
    std::vector<int> order;
    test::require(sink.post([&] { order.push_back(1); }), "first post");
    test::require(sink.post([&] { order.push_back(2); }), "second post");
    test::require(sink.post([&] { order.push_back(3); }), "third post");
    test::require(!sink.post([] {}), "bounded queue rejects overflow");
    test::require(queue.drain() == 2 && queue.pending() == 1,
                  "per-drain work budget");
    queue.drain();
    test::require(order == std::vector<int>{1, 2, 3}, "FIFO delivery");
    sink.post([&] {
      sink.post([&] { order.push_back(5); });
      order.push_back(4);
      test::rejects<std::logic_error>([&] { queue.drain(); },
                                      "recursive drain");
    });
    queue.drain();
    test::require(order.back() == 4, "new work waits for next drain");
    queue.drain();
    test::require(order.back() == 5, "deferred post delivered");
    sink.post([] { throw std::runtime_error("failed completion"); });
    sink.post([&] { order.push_back(6); });
    test::rejects<std::runtime_error>([&] { queue.drain(); },
                                      "completion exception propagates");
    test::require(queue.pending() == 1, "unattempted tail survives");
    queue.drain();
    test::require(order.back() == 6, "failure does not poison future drains");
    std::atomic<bool> wrongThreadRejected{};
    bool workerPosted{};
    std::jthread worker{[&] {
      try {
        queue.drain();
      } catch (const std::logic_error &) {
        wrongThreadRejected = true;
      }
      workerPosted = sink.post([&] { order.push_back(7); });
    }};
    worker.join();
    test::require(workerPosted, "worker can publish");
    test::require(wrongThreadRejected && order.back() == 6,
                  "worker cannot execute owner work");
    queue.drain();
    queue.close();
    test::require(!sink.post([] {}) && queue.drain() == 0,
                  "closed queue rejects work");
    runtime::CompletionSink expired = [] {
      runtime::CompletionQueue temporary;
      return temporary.sink();
    }();
    test::require(!expired.post([] {}), "sink never prolongs queue lifetime");
  });
}
