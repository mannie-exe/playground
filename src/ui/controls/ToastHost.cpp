#include <ui/controls/ToastHost.hpp>

namespace playground::ui {
ToastHost::ToastHost(ToastPresenter present, std::size_t limit,
                     layout::BoxProps box)
    : VStack{{.gap = 8, .childrenAlignment = layout::CrossAlignment::Stretch},
             box},
      _present{std::move(present)}, _limit{limit} {
  if (!_present || !limit || limit > 64)
    throw std::invalid_argument(
        "ToastHost needs presenter and bounded capacity");
  setSemanticProps({.role = SemanticRole::Status, .name = "Notifications"});
}

void ToastHost::post(ToastMessage message) {
  if (message.id.empty() || !std::isfinite(message.timeout) ||
      message.timeout < 0)
    throw std::invalid_argument("Invalid toast identity/timeout");
  for (auto &entry : _entries)
    if (entry.message.id == message.id) {
      entry = {message, message.timeout};
      changed();
      return;
    }
  if (_entries.size() == _limit)
    _entries.pop_front();
  _entries.push_back({message, message.timeout});
  changed();
}

void ToastHost::dismiss(std::string_view id) {
  auto old = _entries.size();
  std::erase_if(_entries, [&](auto &entry) { return entry.message.id == id; });
  if (old != _entries.size())
    changed();
}

void ToastHost::changed() {
  if (!services() || _queued)
    return;
  _queued = true;
  auto self = handle<ToastHost>();
  services()->defer([self] {
    if (auto *node = self.get()) {
      node->_queued = false;
      node->rebuild();
    }
  });
}

void ToastHost::rebuild() {
  std::vector<NodeId> removed;
  for (auto &child : children())
    if (std::none_of(_entries.begin(), _entries.end(), [&](const auto &entry) {
          return entry.node == child->id();
        }))
      removed.push_back(child->id());
  for (auto id : removed)
    remove(id);
  for (auto &entry : _entries) {
    if (entry.node != NodeId{})
      continue;
    auto self = handle<ToastHost>();
    auto id = entry.message.id;
    auto content = _present(entry.message, [self, id] {
      if (auto *node = self.get())
        node->dismiss(id);
    });
    if (!content)
      throw std::invalid_argument("Toast presenter returned no content");
    auto s = content->semanticProps();
    s.role = SemanticRole::Status;
    s.name = entry.message.message;
    content->setSemanticProps(std::move(s));
    entry.node = append(std::move(content)).id();
  }
  for (std::size_t i = 0; i < _entries.size(); ++i)
    for (std::size_t j = i; j < children().size(); ++j)
      if (children()[j]->id() == _entries[i].node) {
        if (i != j)
          moveChild(j, i);
        break;
      }
  _timer.disconnect();
  _lastTick = services()->scheduler->now();
  if (std::any_of(_entries.begin(), _entries.end(),
                  [](auto &e) { return e.message.timeout > 0; })) {
    auto self = handle<ToastHost>();
    _timer = services()->scheduler->schedule(
        .1,
        [self] {
          if (auto *node = self.get())
            node->tick();
        },
        .1);
  }
}

void ToastHost::tick() {
  const auto now = services()->scheduler->now(), elapsed = now - _lastTick;
  _lastTick = now;
  const auto focused = [](auto &&self, const Node &node) -> bool {
    if (node.hasFocus())
      return true;
    for (auto &child : node.children())
      if (self(self, *child))
        return true;
    return false;
  };
  _focused = focused(focused, *this);
  if (_hovered || _focused)
    return;
  for (auto &e : _entries)
    if (e.message.timeout > 0)
      e.remaining -= elapsed;
  auto old = _entries.size();
  std::erase_if(_entries, [](auto &e) {
    return e.message.timeout > 0 && e.remaining <= 0;
  });
  if (old != _entries.size())
    changed();
}

void ToastHost::onAttach(UIServices &) {
  for (auto &entry : _entries)
    entry.node = {};
  changed();
}

void ToastHost::onDefaultEvent(UIEvent &e) {
  if (e.type == EventType::PointerEnter)
    _hovered = true;
  if (e.type == EventType::PointerLeave)
    _hovered = false;
  if ((e.type == EventType::FocusGained ||
       e.type == EventType::FocusWithinGained))
    _focused = true;
  if (e.type == EventType::FocusLost || e.type == EventType::FocusWithinLost)
    _focused = false;
  if (e.type == EventType::InputCancel)
    _focused = _hovered = false;
}
} // namespace playground::ui
