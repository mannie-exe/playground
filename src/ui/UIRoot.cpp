#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>
#include <ui/containers/Popup.hpp>

namespace playground::ui {

std::optional<math::Transform2D> UIRoot::inputInverse(Node &node) {
  auto inverse = node.localTransform().inverse();
  if (!inverse)
    _diagnostics.report(node.id(), LayoutPhase::Input,
                        LayoutIssue::NonInvertibleTransform,
                        "Singular visual transform cannot be hit-tested");
  return inverse;
}

Node *UIRoot::hit(Node &node, math::Point2 local) {
  if (node.visibility() != Visibility::Visible || !node.isArranged() ||
      !node.isInteractionEnabled())
    return nullptr;
  const bool inside = node.containsLocal(local);
  if (node.clipsContent() && !node.containsClip(local))
    return nullptr;
  auto policy = node.hitTestPolicy();
  if (policy == HitTestPolicy::None)
    return nullptr;
  if (inside &&
      (policy == HitTestPolicy::Self ||
       policy == HitTestPolicy::SelfAndChildren) &&
      node.hitTestOverlay(local))
    return &node;
  if (policy == HitTestPolicy::ChildrenOnly ||
      policy == HitTestPolicy::SelfAndChildren) {
    for (auto it = node.children().rbegin(); it != node.children().rend();
         ++it) {
      if ((*it)->isPortal())
        continue;
      if (auto inverse = inputInverse(**it))
        if (auto *found = hit(**it, inverse->mapPoint(local)))
          return found;
    }
  }
  return inside && (policy == HitTestPolicy::Self ||
                    policy == HitTestPolicy::SelfAndChildren)
             ? &node
             : nullptr;
}

bool UIRoot::acceptsInput(const Node &node) noexcept {
  if (!node.isArranged() || node.visibility() != Visibility::Visible ||
      node.hitTestPolicy() == HitTestPolicy::None ||
      !node.isInteractionEnabled() || !node.worldTransform().inverse())
    return false;
  for (auto *parent = node.parent(); parent; parent = parent->parent()) {
    if (!parent->isArranged() || parent->visibility() != Visibility::Visible ||
        !parent->isInteractionEnabled() ||
        parent->hitTestPolicy() == HitTestPolicy::None ||
        parent->hitTestPolicy() == HitTestPolicy::Self)
      return false;
  }
  return true;
}

bool UIRoot::acceptsAction(const Node &node) noexcept {
  for (auto *p = &node; p; p = p->parent())
    if (!p->isArranged() || p->visibility() != Visibility::Visible ||
        !p->isInteractionEnabled())
      return false;
  return true;
}

void UIRoot::clearDirty(Node &node, DirtyFlags flags) noexcept {
  node._dirty = static_cast<DirtyFlags>(static_cast<unsigned>(node._dirty) &
                                        ~static_cast<unsigned>(flags));
  for (auto &child : node._children)
    clearDirty(*child, flags);
}

void UIRoot::collect(Node &node, std::vector<Node *> &nodes) {
  nodes.push_back(&node);
  for (const auto &child : node.children())
    collect(*child, nodes);
}

void UIRoot::direct(Node &node, UIEvent &event) {
  event.phase = EventPhase::Target;
  if (auto inverse = node.worldTransform().inverse())
    event.localPosition = inverse->mapPoint(event.position);
  node.onEvent(event);
  if (!event.defaultPrevented)
    node.onDefaultEvent(event);
}

void UIRoot::synchronizeHover(const UIEvent &event) {
  if (!_content || event.type != EventType::PointerMove)
    return;
  Node *target = presentationHit(event.position);
  auto previous =
      std::find_if(_hovered.begin(), _hovered.end(), [&](const auto &entry) {
        return entry.pointer == event.pointer;
      });
  const NodeId old = previous == _hovered.end() ? NodeId{} : previous->node;
  const NodeId next = target ? target->id() : NodeId{};
  if (old == next)
    return;
  std::vector<Node *> oldPath, newPath;
  for (auto *node = resolve(old); node; node = node->parent())
    oldPath.push_back(node);
  for (auto *node = target; node; node = node->parent())
    newPath.push_back(node);
  while (!oldPath.empty() && !newPath.empty() &&
         oldPath.back() == newPath.back()) {
    oldPath.pop_back();
    newPath.pop_back();
  }
  if (previous == _hovered.end())
    _hovered.push_back({event.pointer, next});
  else
    previous->node = next;
  for (auto *node : oldPath) {
    auto leave = event;
    leave.type = EventType::PointerLeave;
    direct(*node, leave);
  }
  for (auto it = newPath.rbegin(); it != newPath.rend(); ++it) {
    auto enter = event;
    enter.type = EventType::PointerEnter;
    direct(**it, enter);
  }
}

UIRoot::UIRoot(UIServices services, runtime::CompletionQueueProps completions)
    : _services{std::move(services)}, _completions{completions} {
  if (!_services.scheduler)
    _services.scheduler = &_scheduler;
  _services.defer = [this](support::MoveOnlyFunction<void()> work) {
    defer([work = std::move(work)](UIRoot &) mutable { work(); });
  };
  _services.focusAfterLayout = [this](NodeId id) { _pendingFocus = id; };
  _context.stats = &_stats;
  _context.services = &_services;
  _context.diagnostics = &_diagnostics;
  _table->services = &_services;
  _table->stats = &_stats;
  _table->requestFocus = [this](NodeId id) { requestFocus(id); };
}

std::optional<HitResult> UIRoot::hitTest(math::Point2 position) {
  if (!math::isFinite(position))
    throw std::invalid_argument("Invalid hit-test position");
  if (_environment)
    flushLayout(*_environment);
  if (!_content)
    return {};
  Traversal traversal{*_table};
  auto *node = presentationHit(position);
  if (!node)
    return {};
  HitResult result{
      node->handle(), node->worldTransform().inverse()->mapPoint(position), {}};
  for (auto *ancestor = node; ancestor; ancestor = ancestor->parent())
    result.path.push_back(ancestor->id());
  return result;
}

std::vector<NodeInspection> UIRoot::inspectTree() const {
  std::vector<NodeInspection> result;
  if (!_content)
    return result;
  std::vector<Node *> nodes;
  collect(*_content, nodes);
  for (auto *node : nodes)
    result.push_back({node->id(),
                      node->parent() ? node->parent()->id() : NodeId{},
                      node->nodeProps().debugName, node->bounds(),
                      node->dirtyFlags(), node->semanticProps()});
  return result;
}

void UIRoot::setContent(std::unique_ptr<Node> content) {
  if (_table->traversals || _table->lifecycleCallbacks)
    throw std::logic_error("Defer UI root replacement during traversal");
  if (content && (content->_parent || !content->_table.expired()))
    throw std::invalid_argument("Root content must be detached");
  if (content)
    content->attach(_table);
  if (_content)
    _content->detach();
  _content = std::move(content);
  // Modal restoration belongs to the old tree, never its replacement.
  _modal = {};
  _modalHistory.clear();
  _pendingFocus.reset();
  _hovered.clear();
  _claimsRevision.reset();
  _overlays.clear();
  _dismissedPointers.clear();
  _table->layoutDirty = _table->paintDirty = true;
  ++_table->revision;
}

void UIRoot::flushMutations() {
  if (_table->traversals)
    throw std::logic_error("Cannot flush mutations during UI traversal");
  if (_flushingMutations)
    throw std::logic_error("Cannot recursively flush UI mutations");
  _flushingMutations = true;

  struct FlushGuard {
    bool &active;

    ~FlushGuard() { active = false; }
  } guard{_flushingMutations};

  // Newly enqueued work waits for the next boundary; a callback cannot spin
  // forever.
  const auto count = _deferred.size();
  for (std::size_t i{}; i < count; ++i) {
    auto command = std::move(_deferred.front());
    _deferred.pop_front();
    command(*this);
  }
}

void UIRoot::flushLayout(math::Size2 viewport,
                         layout::LayoutDirection direction,
                         std::uint64_t environmentRevision) {
  flushLayout(LayoutEnvironment{.viewport = viewport,
                                .direction = direction,
                                .revision = environmentRevision});
}

math::Size2 UIRoot::preferredSize(math::Size2 maximum, math::Vec2f pixelScale) {
  UIWorkTiming::Scope timing{_timing, UIWorkPhase::Layout};
  if (_table->traversals || _table->lifecycleCallbacks)
    throw std::logic_error("Cannot measure preferred size during UI traversal");
  if (!math::isFinite(maximum) || !math::hasArea(maximum) ||
      !math::isFinite(pixelScale) || !math::hasArea(pixelScale))
    throw std::invalid_argument("Invalid preferred-size offer");
  if (!_content)
    return {};
  auto context = _context;
  context.pixelScale = pixelScale;
  // A custom measurement can replace offer-dependent derived state.
  _table->layoutDirty = true;
  Traversal traversal{*_table};
  return _content->measure(context, {{0, maximum.width}, {0, maximum.height}})
      .size;
}

void UIRoot::flushChanges() {
  if (_table->traversals)
    throw std::logic_error("Cannot flush changes during traversal");
  std::vector<Node *> nodes;
  if (_table->dirtyFallback) {
    if (_content)
      collect(*_content, nodes);
  } else
    for (auto id : _table->dirty)
      if (auto *node = resolve(id))
        nodes.push_back(node);
  for (auto *node : nodes)
    node->_queued = false;
  _table->dirty.clear();
  _table->dirtyFallback = false;
  Traversal traversal{*_table};
  try {
    for (auto *node : nodes)
      if (any(node->_pendingChanges)) {
        const ChangeSet change{
            std::exchange(node->_pendingChanges, DirtyFlags::None),
            node->sourceRevision()};
        node->onPropsChanged(change);
        node->_changes.emit(change);
      }
  } catch (...) {
    _table->dirtyFallback = true;
    throw;
  }
}

void UIRoot::prepare(PrepareContext context) {
  UIWorkTiming::Scope timing{_timing, UIWorkPhase::Prepare};
  if (_environment)
    flushLayout(*_environment);
  Traversal traversal{*_table};
  context.stats = &_stats;
  if (_content)
    _content->prepare(context);
  for (auto id : _overlays)
    if (auto *node = resolve(id))
      node->prepare(context);
}

void UIRoot::update(double seconds) {
  if (_updating)
    throw std::logic_error("Cannot recursively update UI runtime");
  _updating = true;

  struct UpdateGuard {
    bool &active;

    ~UpdateGuard() { active = false; }
  } guard{_updating};

  {
    UIWorkTiming::Scope timing{_timing, UIWorkPhase::Completions};
    _completions.drain();
  }
  _services.scheduler->advance(seconds);
  flushMutations();
  flushChanges();
  if (_environment)
    flushLayout(*_environment);
}

void UIRoot::render(PaintContext &context) const {
  UIWorkTiming::Scope timing{_timing, UIWorkPhase::Paint};
  Traversal traversal{*_table};
  const auto revision = _table->revision;
  if (_content)
    _content->render(context);
  PaintScope overlayScope{context};
  context.clip({{}, _viewport});
  for (auto id : _overlays)
    if (auto *node = dynamic_cast<Portal *>(resolve(id));
        node && node->popupProps().open) {
      if (node->popupProps().backdrop)
        context.fill({{}, _viewport}, *node->popupProps().backdrop);
      node->render(context, true);
    }
  if (_table->revision == revision) {
    if (_content)
      clearDirty(*_content, DirtyFlags::Paint);
    _table->paintDirty = false;
  }
}

void UIRoot::focusNext(bool reverse) {
  if (!_content)
    return;
  std::vector<Node *> candidates;
  auto *scope = navigationScope();
  if (auto *current = resolve(_table->focused)) {
    const auto neighbor = reverse ? current->inputProps().neighbors.previous
                                  : current->inputProps().neighbors.next;
    if (neighbor)
      if (auto *next = resolve(*neighbor); next && next->isFocusable() &&
                                           acceptsAction(*next) &&
                                           withinScope(*next, scope)) {
        requestFocus(*neighbor);
        return;
      }
  }
  collect(*(scope ? scope : _content.get()), candidates);
  const auto sequentialTarget = [scope](Node *node) {
    auto *target = node;
    for (auto *parent = node ? node->parent() : nullptr; parent && target;
         parent = parent->parent()) {
      if (scope && parent == scope->parent())
        break;
      target = parent->sequentialFocusTarget(*target);
    }
    return target;
  };
  std::erase_if(candidates, [&](Node *node) {
    return !node->isFocusable() || !acceptsAction(*node) ||
           sequentialTarget(node) != node;
  });
  if (candidates.empty()) {
    _table->focused = {};
    return;
  }
  const auto it =
      std::find_if(candidates.begin(), candidates.end(), [&](Node *node) {
        return node == sequentialTarget(resolve(_table->focused));
      });
  if (scope && !scope->inputProps().wrapNavigation && it != candidates.end() &&
      ((reverse && it == candidates.begin()) ||
       (!reverse && it + 1 == candidates.end())))
    return;
  const std::size_t index =
      it == candidates.end()
          ? (reverse ? candidates.size() - 1 : 0)
          : (static_cast<std::size_t>(it - candidates.begin()) +
             (reverse ? candidates.size() - 1 : 1)) %
                candidates.size();
  requestFocus(candidates[index]->id());
}

void UIRoot::requestFocus(NodeId id) {
  auto *node = resolve(id);
  if (_modal != NodeId{} && node && !withinScope(*node, resolve(_modal)))
    return;
  if (node && (!node->isFocusable() || !acceptsAction(*node)))
    return;
  if (_table->focused == id)
    return;
  Traversal traversal{*_table};
  auto *old = resolve(_table->focused);
  if (old) {
    UIEvent event{.type = EventType::FocusLost};
    direct(*old, event);
    event.type = EventType::FocusWithinLost;
    for (auto *parent = old->parent(); parent; parent = parent->parent())
      if (!node || !withinScope(*node, parent))
        direct(*parent, event);
  }
  _table->focused = node ? id : NodeId{};
  if (node) {
    UIEvent event{.type = EventType::FocusGained};
    direct(*node, event);
    event.type = EventType::FocusWithinGained;
    for (auto *parent = node->parent(); parent; parent = parent->parent())
      if (!old || !withinScope(*old, parent))
        direct(*parent, event);
    for (auto *p = node->parent(); p; p = p->parent())
      if (p->isPortal())
        break;
      else if (auto *scroll = dynamic_cast<ScrollView *>(p))
        scroll->scrollIntoView(*node);
  }
}

void UIRoot::dispatch(UIEvent &event) {
  UIWorkTiming::Scope timing{_timing, UIWorkPhase::Input};
  if (_environment)
    flushLayout(*_environment);
  auto *scope = navigationScope();
  {
    Traversal traversal{*_table};
    const auto route = [&] {
      if (routeOverlayDismissal(event))
        return;
      if (event.type == EventType::PointerLeave) {
        for (const auto &entry : _hovered)
          for (auto *node = resolve(entry.node); node; node = node->parent()) {
            auto leave = event;
            direct(*node, leave);
          }
        _hovered.clear();
        return;
      }
      // Cancel stale captures before selecting a target. Callbacks may
      // enqueue structural work, but cannot destroy nodes until traversal
      // completes.
      const auto captures = _table->captures;
      for (const auto &capture : captures) {
        auto *node = resolve(capture.node);
        if (!node || !acceptsInput(*node)) {
          std::erase_if(_table->captures, [&](const auto &current) {
            return current.pointer == capture.pointer &&
                   current.node == capture.node;
          });
          if (node) {
            UIEvent cancel{.type = EventType::PointerCancel,
                           .pointer = capture.pointer};
            direct(*node, cancel);
          }
        }
      }
      if (auto *focused = resolve(_table->focused);
          !focused || !focused->isFocusable() || !acceptsAction(*focused))
        requestFocus({});
      if (!_content)
        return;
      synchronizeHover(event);
      Node *target{};
      if (event.type == EventType::TextInput ||
          event.type == EventType::TextEditing ||
          event.type == EventType::KeyDown || event.type == EventType::KeyUp ||
          event.type == EventType::FocusGained) {
        target = resolve(_table->focused);
      } else {
        for (const auto &capture : _table->captures)
          if (capture.pointer == event.pointer)
            target = resolve(capture.node);
        if (!target) {
          target = presentationHit(event.position);
        }
      }
      if (event.type == EventType::FocusLost ||
          event.type == EventType::InputCancel) {
        for (const auto &entry : _hovered)
          for (auto *node = resolve(entry.node); node; node = node->parent()) {
            UIEvent leave{.type = EventType::PointerLeave,
                          .pointer = entry.pointer};
            direct(*node, leave);
          }
        _hovered.clear();
        // Cancellation is a separate event; do not let a release activate a
        // stale press.
        const auto captures = _table->captures;
        for (const auto &capture : captures)
          if (auto *node = resolve(capture.node)) {
            UIEvent cancel{.type = EventType::PointerCancel,
                           .pointer = capture.pointer};
            direct(*node, cancel);
          }
        _table->captures.clear();
        if (auto *focused = resolve(_table->focused)) {
          direct(*focused, event);
          auto within = event;
          within.type = EventType::FocusWithinLost;
          for (auto *parent = focused->parent(); parent;
               parent = parent->parent())
            direct(*parent, within);
        }
        if (event.type == EventType::FocusLost)
          _table->focused = {};
        return;
      }
      if (event.type == EventType::KeyDown && event.logicalKey == Key::Tab &&
          !target && _interaction.sequentialNavigation) {
        focusNext(event.shift);
        event.handled = _table->focused != NodeId{} || _modal != NodeId{};
        return;
      }
      if (!target)
        return;
      if (_modal != NodeId{} && !withinScope(*target, scope)) {
        event.handled = true;
        return;
      }
      std::vector<Node *> path;
      for (auto *node = target; node; node = node->parent())
        path.push_back(node);
      auto deliver = [&](Node &node, EventPhase phase) {
        event.phase = phase;
        if (auto inverse = node.worldTransform().inverse())
          event.localPosition = inverse->mapPoint(event.position);
        node.onEvent(event);
      };
      for (std::size_t i = path.size(); i > 1 && !event.propagationStopped; --i)
        deliver(*path[i - 1], EventPhase::Capture);
      if (!event.propagationStopped)
        deliver(*target, EventPhase::Target);
      for (std::size_t i = 1; i < path.size() && !event.propagationStopped; ++i)
        deliver(*path[i], EventPhase::Bubble);
      if (!event.defaultPrevented) {
        if (event.type == EventType::KeyDown && event.logicalKey == Key::Tab &&
            _interaction.sequentialNavigation) {
          focusNext(event.shift);
          event.handled = true;
        } else {
          for (auto *node : path) {
            event.phase =
                node == target ? EventPhase::Target : EventPhase::Bubble;
            if (auto inverse = node->worldTransform().inverse())
              event.localPosition = inverse->mapPoint(event.position);
            node->onDefaultEvent(event);
            if (event.defaultPrevented || event.propagationStopped)
              break;
          }
        }
      }
    };
    route();
    if (!event.handled && !event.defaultPrevented &&
        !event.propagationStopped && event.type == EventType::KeyDown &&
        _interaction.directionalNavigation) {
      math::Vec2f direction{};
      if (event.logicalKey == Key::Left)
        direction.x = -1;
      if (event.logicalKey == Key::Right)
        direction.x = 1;
      if (event.logicalKey == Key::Up)
        direction.y = -1;
      if (event.logicalKey == Key::Down)
        direction.y = 1;
      if (direction != math::Vec2f{}) {
        event.handled = focusDirection(direction);
      }
    }
  }
  flushMutations();
  flushChanges();
}

} // namespace playground::ui
