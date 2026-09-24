#include <memory>
#include <thread>

#include <support/Test.hpp>
#include <ui/UIRoot.hpp>
#include <ui/containers/Box.hpp>

using namespace playground;

int main() {
  return test::run([] {
    auto root = std::make_unique<ui::UIRoot>();
    root->setContent(std::make_unique<ui::Box>());
    auto handle = root->content()->handle();
    const auto revision = root->content()->sourceRevision();
    auto sink = root->completionSink();
    int count{};
    const auto ownerThread = std::this_thread::get_id();
    bool posted{};
    std::jthread worker([&] {
      posted = sink.post(handle, revision, [&](ui::Node &) {
        test::require(std::this_thread::get_id() == ownerThread,
                      "completion runs on UI thread, not posting worker");
        ++count;
      });
    });
    worker.join();
    test::require(posted && count == 0,
                  "posting queues but does not execute completion");
    root->update(0);
    test::require(count == 1, "update delivers matching completion");

    std::vector<int> order;
    sink.post(handle, revision, [&](ui::Node &) {
      order.push_back(1);
      sink.post(handle, revision, [&](ui::Node &) { order.push_back(3); });
      throw std::runtime_error("completion failed");
    });
    sink.post(handle, revision, [&](ui::Node &) { order.push_back(2); });
    test::rejects<std::runtime_error>([&] { root->update(0); },
                                      "completion failure reaches caller");
    test::require(order == std::vector<int>{1},
                  "failed callback attempted once");
    root->update(0);
    test::require(order == std::vector<int>{1, 2, 3},
                  "unexecuted tail survives ahead of newly posted work");
    sink.post(handle, revision, [&](ui::Node &) {
      test::rejects<std::logic_error>([&] { root->update(0); },
                                      "recursive update rejected");
      sink.post(handle, revision, [&](ui::Node &) { order.push_back(5); });
      order.push_back(4);
    });
    root->update(0);
    test::require(order.back() == 4, "new work waits until next update");
    root->update(0);
    test::require(order.back() == 5, "update guard resets after exceptions");
    sink.post(handle, revision, [&](ui::Node &) { ++count; });
    root->content()->setBoxProps({.padding = math::Insets::all(2)});
    root->update(0);
    test::require(count == 1, "stale source revision discarded");
    const auto current = root->content()->sourceRevision();
    sink.post(handle, current, [&](ui::Node &) { ++count; });
    root->setContent({});
    root->update(0);
    test::require(count == 1, "removed node completion discarded");
    root.reset();
    test::require(!sink.post(handle, current, [](ui::Node &) {}),
                  "posting to expired root rejected");
  });
}
