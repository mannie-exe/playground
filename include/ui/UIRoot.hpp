#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <input/InputMap.hpp>
#include <runtime/CompletionQueue.hpp>
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

class CompletionSink {
  runtime::CompletionSink _sink;

public:
  explicit CompletionSink(runtime::CompletionSink sink)
      : _sink{std::move(sink)} {}

  template <typename T, typename Callback>
  bool post(NodeHandle<T> handle, Revision revision, Callback callback) const {
    return _sink.post([handle, revision,
                       callback = std::move(callback)]() mutable {
      if (auto *node = handle.get(); node && node->sourceRevision() == revision)
        callback(*node);
    });
  }
};

class UIRoot {
  Scheduler _scheduler;
  UIServices _services;
  std::shared_ptr<detail::NodeTable> _table{
      std::make_shared<detail::NodeTable>()};
  std::unique_ptr<Node> _content;

  std::deque<support::MoveOnlyFunction<void(UIRoot &)>> _deferred;
  bool _flushingMutations{};
  bool _updating{};
  runtime::CompletionQueue _completions;
  std::vector<detail::NodeTable::Capture> _hovered;

  math::Size2 _viewport{};
  std::optional<LayoutEnvironment> _environment;
  MeasureContext _context;
  UIWorkStats _stats;
  InteractionProps _interaction;
  NodeId _modal;
  std::optional<NodeId> _pendingFocus;
  std::vector<NodeId> _overlays;
  std::vector<std::uint64_t> _dismissedPointers;
  void layoutOverlays();
  Node *presentationHit(math::Point2);
  bool routeOverlayDismissal(UIEvent &);
  std::vector<std::pair<NodeId, NodeId>> _modalHistory;
  std::optional<std::pair<std::uint64_t, std::uint64_t>> _claimsRevision;
  NodeId _claimsFocus;
  input::InputClaims _claims;
  const std::uint64_t _workId{nextUIWorkId()};
  mutable UIWorkTiming _timing;
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
  static bool acceptsAction(const Node &node) noexcept;

  static void clearDirty(Node &node, DirtyFlags flags) noexcept;
  static void collect(Node &node, std::vector<Node *> &nodes);
  static void direct(Node &node, UIEvent &event);
  void synchronizeHover(const UIEvent &event);
  Node *navigationScope();
  bool withinScope(const Node &node, const Node *scope) const;

public:
  explicit UIRoot(UIServices services = {},
                  runtime::CompletionQueueProps completions = {});

  struct PublicationKey {
    std::uint64_t root{}, revision{}, geometry{};
    NodeId focused;
    bool operator==(const PublicationKey &) const = default;
  };

  PublicationKey publicationKey() const noexcept {
    return {_workId, _table->revision, _table->geometryRevision, focusedNode()};
  }

  UIWorkTiming::Scope publicationScope() {
    return {_timing, UIWorkPhase::Publication};
  }

  void recordPublication(bool cached) noexcept {
    if (cached)
      ++_stats.publicationHits;
    else
      ++_stats.publications;
  }

  ~UIRoot() {
    _completions.close();
    if (_content)
      _content->detach();
  }

  UIRoot(const UIRoot &) = delete;
  UIRoot &operator=(const UIRoot &) = delete;

  Node *content() const noexcept { return _content.get(); }

  Node *resolve(NodeId id) const noexcept { return _table->resolve(id); }

  const UIWorkStats &stats() const noexcept { return _stats; }

  UIWorkSample workSample() const noexcept {
    return {_workId, _stats, _timing.totals()};
  }

  void setTimingEnabled(bool enabled) noexcept { _timing.setEnabled(enabled); }

  const LayoutDiagnostics &diagnostics() const noexcept { return _diagnostics; }

  math::Size2 viewport() const noexcept { return _viewport; }

  const std::optional<LayoutEnvironment> &environment() const noexcept {
    return _environment;
  }

  UIServices &services() noexcept { return _services; }

  void setTheme(ThemePalette value) {
    if (_services.theme == value)
      return;
    _services.theme = std::move(value);
    if (_content)
      _content->refreshTheme();
  }

  CompletionSink completionSink() const {
    return CompletionSink{_completions.sink()};
  }

  std::optional<HitResult> hitTest(math::Point2 position);

  bool needsPaint() const noexcept { return _table->paintDirty; }

  bool needsUpdate() const {
    return _completions.pending() || !_deferred.empty() ||
           _table->layoutDirty || !_table->dirty.empty() ||
           _table->dirtyFallback;
  }

  std::optional<double> nextUpdateDelay() {
    return _services.scheduler->nextDelay();
  }

  void setWakeCallback(std::function<void()> callback) {
    _completions.setWakeCallback(std::move(callback));
  }

  void requestPaint() noexcept {
    _table->paintDirty = true;
    ++_table->revision;
  }

  std::vector<NodeInspection> inspectTree() const;

  void setContent(std::unique_ptr<Node> content);

  void defer(support::MoveOnlyFunction<void(UIRoot &)> command) {
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
    UIWorkTiming::Scope timing{_timing, UIWorkPhase::Layout};
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
    if (!_table->layoutDirty && _content->isArranged()) {
      Traversal traversal{*_table};
      layoutOverlays();
      if (auto focus = std::exchange(_pendingFocus, {}))
        requestFocus(*focus);
      return;
    }
    Traversal traversal{*_table};
    const auto revision = _table->revision;
    const auto available =
        math::inset({{}, environment.viewport}, environment.usableInsets);
    const auto constraints = layout::SizeConstraints::tight(available.size);
    _content->measure(_context, constraints);
    _content->arrange(_context, available);
    // Snapshot identities: callbacks may append work, and detach invalidates
    // IDs.
    const auto boundaries = _table->layoutBoundaries;
    for (const auto id : boundaries)
      if (auto *boundary = resolve(id)) {
        bool covered = false;
        for (auto *parent = boundary->parent(); parent;
             parent = parent->parent())
          if (std::find(boundaries.begin(), boundaries.end(), parent->id()) !=
              boundaries.end()) {
            covered = true;
            break;
          }
        if (covered)
          continue;
        boundary->arrange(_context, boundary->bounds());
        for (auto *parent = boundary->parent(); parent;
             parent = parent->parent())
          parent->refreshOverflow();
      }
    layoutOverlays();
    if (auto focus = std::exchange(_pendingFocus, {}))
      requestFocus(*focus);
    if (_table->revision == revision) {
      clearDirty(*_content, DirtyFlags::Measure | DirtyFlags::Arrange);
      _table->layoutDirty = false;
      _table->layoutBoundaries.clear();
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
  input::InputClaims inputClaims();
  // True when navigation belongs to UI, including an occupied focus boundary.
  // False lets an AfterUI application action handle otherwise unused arrows.
  bool focusDirection(math::Vec2f direction);

  NodeId focusedNode() const noexcept { return _table->focused; }

  const InteractionProps &interactionProps() const noexcept {
    return _interaction;
  }

  void setInteractionProps(InteractionProps props) {
    props.validate();
    _interaction = props;
  }

  ActionResult performAction(NodeId target, const UIAction &,
                             ActionSource = ActionSource::Program);
  SemanticSnapshot semanticSnapshot();
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
