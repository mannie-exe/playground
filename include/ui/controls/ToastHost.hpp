#pragma once
#include <deque>

#include <ui/containers/Stack.hpp>

namespace playground::ui {
struct ToastMessage {
  std::string id, message;
  double timeout{5}; // zero stays until dismissed
};

using ToastPresenter = std::function<std::unique_ptr<Node>(
    const ToastMessage &, std::function<void()>)>;

class ToastHost : public VStack {
  struct Entry {
    ToastMessage message;
    double remaining;
    NodeId node{};
  };

  std::deque<Entry> _entries;
  ToastPresenter _present;
  std::size_t _limit;
  TimerHandle _timer;
  bool _hovered{}, _focused{}, _queued{};
  double _lastTick{};
  void changed();
  void rebuild();
  void tick();

protected:
  void onAttach(UIServices &) override;

  void onDetach() noexcept override {
    _timer.disconnect();
    _queued = false;
    _hovered = _focused = false;
  }

  void onDefaultEvent(UIEvent &) override;

public:
  explicit ToastHost(ToastPresenter, std::size_t limit = 4,
                     layout::BoxProps = {});
  void post(ToastMessage);
  void dismiss(std::string_view id);

  std::size_t size() const noexcept { return _entries.size(); }
};
} // namespace playground::ui
