#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <ui/Node.hpp>

namespace playground::ui {

struct NodeInspection {
  NodeId id;
  NodeId parent;
  std::optional<std::string> name;
  math::Rect bounds;
  DirtyFlags dirty;
  SemanticProps semantics;
};

namespace detail {
struct CompletionMailbox {
  std::mutex mutex;
  std::deque<std::move_only_function<void()>> pending;
};
} // namespace detail

class CompletionSink {
  std::weak_ptr<detail::CompletionMailbox> _mailbox;

public:
  explicit CompletionSink(std::weak_ptr<detail::CompletionMailbox> mailbox)
      : _mailbox{std::move(mailbox)} {}
  template <typename T, typename Callback>
  bool post(NodeHandle<T> handle, Revision revision, Callback callback) const {
    auto mailbox = _mailbox.lock();
    if (!mailbox)
      return false;
    std::lock_guard lock{mailbox->mutex};
    mailbox->pending.push_back([handle, revision,
                                callback = std::move(callback)]() mutable {
      if (auto *node = handle.get(); node && node->sourceRevision() == revision)
        callback(*node);
    });
    return true;
  }
};

class UIRoot {
  Scheduler _scheduler;
  UIServices _services;
  std::shared_ptr<detail::NodeTable> _table{
      std::make_shared<detail::NodeTable>()};
  std::unique_ptr<Node> _content;

  std::deque<std::move_only_function<void(UIRoot &)>> _deferred;
  bool _flushingMutations{};
  bool _updating{};
  std::shared_ptr<detail::CompletionMailbox> _completions{
      std::make_shared<detail::CompletionMailbox>()};
  std::vector<detail::NodeTable::Capture> _hovered;

  math::Size2 _viewport{};
  std::optional<LayoutEnvironment> _environment;
  MeasureContext _context;
  LayoutStats _stats;
  LayoutDiagnostics _diagnostics;

  class Traversal {
    detail::NodeTable &_table;

  public:
    explicit Traversal(detail::NodeTable &table) : _table{table} {
      ++_table.traversals;
    }
    ~Traversal() { --_table.traversals; }

    Traversal(const Traversal &) = delete;
    Traversal &operator=(const Traversal &) = delete;
  };

  std::optional<math::Transform2D> inputInverse(Node &node);

  Node *hit(Node &node, math::Point2 local);

  static bool acceptsInput(const Node &node) noexcept;

  static void clearDirty(Node &node, DirtyFlags flags) noexcept;
  static void collect(Node &node, std::vector<Node *> &nodes);
  static void direct(Node &node, UIEvent &event);
  void synchronizeHover(const UIEvent &event);

public:
  explicit UIRoot(UIServices services = {});
  ~UIRoot() {
    if (_content)
      _content->detach();
  }
  UIRoot(const UIRoot &) = delete;
  UIRoot &operator=(const UIRoot &) = delete;

  Node *content() const noexcept { return _content.get(); }
  Node *resolve(NodeId id) const noexcept { return _table->resolve(id); }
  const LayoutStats &stats() const noexcept { return _stats; }
  const LayoutDiagnostics &diagnostics() const noexcept { return _diagnostics; }
  math::Size2 viewport() const noexcept { return _viewport; }
  const std::optional<LayoutEnvironment> &environment() const noexcept {
    return _environment;
  }
  UIServices &services() noexcept { return _services; }
  CompletionSink completionSink() const { return CompletionSink{_completions}; }
  std::optional<HitResult> hitTest(math::Point2 position);
  bool needsPaint() const noexcept { return _table->paintDirty; }
  void requestPaint() noexcept { _table->paintDirty = true; }
  std::vector<NodeInspection> inspectTree() const;

  void setContent(std::unique_ptr<Node> content);

  void defer(std::move_only_function<void(UIRoot &)> command) {
    if (command)
      _deferred.push_back(std::move(command));
  }

  void flushMutations();

  // Measures a preferred logical client size, including root insets, without
  // arranging the tree or requesting a platform window change.
  math::Size2 preferredSize(math::Size2 maximum,
                            math::Vec2f pixelScale = {1, 1});

  void flushLayout(
      math::Size2 viewport,
      layout::LayoutDirection direction = layout::LayoutDirection::LeftToRight,
      std::uint64_t environmentRevision = 0);

  template <std::same_as<LayoutEnvironment> Environment>
  void flushLayout(const Environment &environment) {
    if (_table->traversals || _table->lifecycleCallbacks)
      throw std::logic_error(
          "Cannot flush layout during UI traversal or lifecycle callbacks");
    environment.validate();
    const bool changed = !_environment || *_environment != environment;
    _environment = environment;
    _viewport = environment.viewport;
    _context.direction = environment.direction;
    _context.environmentRevision = environment.revision;
    _context.pixelScale = environment.pixelScale;
    if (changed) {
      _table->layoutDirty = _table->paintDirty = true;
      if (_content)
        _content->invalidateLayout();
    }
    if (!_content)
      return;
    if (!_table->layoutDirty && _content->isArranged())
      return;
    Traversal traversal{*_table};
    const auto revision = _table->revision;
    const auto available =
        math::inset({{}, environment.viewport}, environment.usableInsets);
    const auto constraints = layout::SizeConstraints::tight(available.size);
    _content->measure(_context, constraints);
    _content->arrange(_context, available);
    if (_table->revision == revision) {
      clearDirty(*_content, DirtyFlags::Measure | DirtyFlags::Arrange);
      _table->layoutDirty = false;
    }
  }

  void flushChanges();

  void prepare() {
    prepare(PrepareContext{.pixelScale = _environment ? _environment->pixelScale
                                                      : math::Vec2f{1, 1}});
  }
  void prepare(PrepareContext context);

  void update(double seconds);

  void render(PaintContext &context) const;

  void focusNext(bool reverse = false);
  void requestFocus(NodeId id);
  void capturePointer(NodeId id, std::uint64_t pointer) {
    if (auto *node = resolve(id); node && acceptsInput(*node))
      node->capturePointer(pointer);
  }
  void releasePointer(NodeId id, std::uint64_t pointer) {
    if (auto *node = resolve(id))
      node->releasePointer(pointer);
  }
  template <typename Container, typename Placement>
  Node &reparent(NodeId child, Container &from, Container &to,
                 Placement placement) {
    if (resolve(from.id()) != &from || resolve(to.id()) != &to)
      throw std::invalid_argument(
          "Reparent containers must belong to this UI root");
    return to.reparentFrom(from, child, std::move(placement));
  }

  void dispatch(UIEvent &event);
};

} // namespace playground::ui
