#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <ui/TextEdit.hpp>
#include <ui/UIRoot.hpp>
#include <ui/collections/ScrollView.hpp>

namespace playground::ui {
void RangeValue::validate() const {
  if (!std::isfinite(value) || !std::isfinite(minimum) ||
      !std::isfinite(maximum) || !std::isfinite(step) ||
      !std::isfinite(maximum - minimum) || minimum > maximum ||
      value < minimum || value > maximum || step <= 0)
    throw std::invalid_argument("Invalid numeric range");
}

double RangeValue::adjusted(int direction) const {
  validate();
  return std::clamp(value + (direction > 0   ? step
                             : direction < 0 ? -step
                                             : 0),
                    minimum, maximum);
}

bool UIRoot::withinScope(const Node &node, const Node *scope) const {
  if (!scope)
    return true;
  for (auto *p = &node; p; p = p->parent())
    if (p == scope)
      return true;
  return false;
}

Node *UIRoot::navigationScope() {
  std::vector<Node *> modals;
  if (_content) {
    std::vector<Node *> nodes;
    collect(*_content, nodes);
    for (auto *node : nodes)
      if (node->inputProps().modal && acceptsAction(*node))
        modals.push_back(node);
  }
  std::size_t common{};
  while (common < modals.size() && common < _modalHistory.size() &&
         modals[common]->id() == _modalHistory[common].first)
    ++common;
  while (_modalHistory.size() > common) {
    const auto restore = _modalHistory.back().second;
    _modalHistory.pop_back();
    _modal = _modalHistory.empty() ? NodeId{} : _modalHistory.back().first;
    requestFocus(restore);
    if (!resolve(_table->focused)) {
      std::vector<Node *> candidates;
      if (auto *scope = resolve(_modal))
        collect(*scope, candidates);
      else if (_content)
        collect(*_content, candidates);
      for (auto *candidate : candidates)
        if (candidate->isFocusable() && acceptsAction(*candidate)) {
          requestFocus(candidate->id());
          break;
        }
    }
  }
  for (; common < modals.size(); ++common) {
    _modalHistory.push_back(
        {modals[common]->id(),
         modals[common]->inputProps().returnFocus.value_or(_table->focused)});
    _modal = modals[common]->id();
  }
  Node *modal = modals.empty() ? nullptr : modals.back();
  if (modal) {
    Traversal traversal{*_table};
    const auto captures = _table->captures;
    for (const auto &capture : captures)
      if (auto *node = resolve(capture.node);
          node && !withinScope(*node, modal)) {
        std::erase_if(_table->captures, [&](const auto &item) {
          return item.pointer == capture.pointer;
        });
        UIEvent cancel{.type = EventType::PointerCancel,
                       .pointer = capture.pointer};
        direct(*node, cancel);
      }
  }
  if (modal && (!resolve(_table->focused) ||
                !withinScope(*resolve(_table->focused), modal))) {
    requestFocus({});
    std::vector<Node *> nodes;
    collect(*modal, nodes);
    if (modal->inputProps().initialFocus)
      if (auto *initial = resolve(*modal->inputProps().initialFocus);
          initial && withinScope(*initial, modal) && initial->isFocusable() &&
          acceptsAction(*initial))
        requestFocus(initial->id());
    for (auto *node : nodes)
      if (_table->focused == NodeId{} && node->isFocusable() &&
          acceptsAction(*node)) {
        requestFocus(node->id());
        break;
      }
  }
  if (modal)
    return modal;
  for (auto *p = resolve(_table->focused); p; p = p->parent())
    if (p->inputProps().focusScope)
      return p;
  return _content.get();
}

input::InputClaims UIRoot::inputClaims() {
  const auto revision = std::pair{_table->revision, _table->geometryRevision};
  if (_claimsRevision != revision || _claimsFocus != _table->focused) {
    navigationScope();
    const bool modal = _modal != NodeId{};
    input::InputClaims next{
        .keyboard = modal, .pointer = modal, .gamepad = modal};
    if (auto *focused = resolve(_table->focused);
        focused && focused->isFocusable() && acceptsAction(*focused))
      next.keyboard |= dynamic_cast<TextInputClient *>(focused) != nullptr;
    _claims = std::move(next);
    // A callback changing the tree during navigation must force another query.
    _claimsRevision = revision;
    _claimsFocus = _table->focused;
  }
  auto result = _claims;
  for (const auto &capture : _table->captures)
    if (auto *node = resolve(capture.node); node && acceptsInput(*node))
      result.capturedPointers.push_back(capture.pointer);
  return result;
}

bool UIRoot::focusDirection(math::Vec2f direction) {
  if (!_interaction.directionalNavigation || !math::isFinite(direction) ||
      direction == math::Vec2f{})
    return false;
  auto *scope = navigationScope();
  auto *current = resolve(_table->focused);
  if (current && (!current->isFocusable() || !acceptsAction(*current) ||
                  !withinScope(*current, scope))) {
    requestFocus({});
    current = nullptr;
  }
  if (!current) {
    focusNext();
    return _table->focused != NodeId{} || _modal != NodeId{};
  }
  const auto &neighbors = current->inputProps().neighbors;
  const auto neighbor = direction.x < 0   ? neighbors.left
                        : direction.x > 0 ? neighbors.right
                        : direction.y < 0 ? neighbors.up
                                          : neighbors.down;
  if (neighbor)
    if (auto *next = resolve(*neighbor); next && next->isFocusable() &&
                                         acceptsAction(*next) &&
                                         withinScope(*next, scope)) {
      requestFocus(*neighbor);
      return true;
    }
  const auto center = [](const Node &node) {
    return node.worldTransform().mapPoint(
        {node.bounds().w() / 2, node.bounds().h() / 2});
  };
  const auto origin = center(*current);
  std::vector<Node *> nodes;
  if (scope)
    collect(*scope, nodes);
  Node *best{};
  float score = std::numeric_limits<float>::infinity();
  for (auto *node : nodes) {
    if (node == current || !node->isFocusable() || !acceptsAction(*node))
      continue;
    const auto delta = center(*node) - origin;
    const float ahead = delta.x * direction.x + delta.y * direction.y;
    if (ahead <= 0)
      continue;
    const float across =
        std::abs(delta.x * direction.y - delta.y * direction.x);
    const float candidate = ahead + across * 4;
    if (candidate < score) {
      score = candidate;
      best = node;
    }
  }
  if (best)
    requestFocus(best->id());
  return true;
}

ActionResult UIRoot::performAction(NodeId target, const UIAction &action,
                                   ActionSource source) {
  auto *node = resolve(target);
  if (!node)
    return ActionResult::Stale;
  auto *scope = navigationScope();
  if (!acceptsAction(*node) ||
      (_modal != NodeId{} && !withinScope(*node, scope)))
    return ActionResult::Unavailable;
  if (source == ActionSource::Assistive)
    for (auto *p = node; p; p = p->parent())
      if (p->semanticProps().exposure == SemanticExposure::HiddenSubtree)
        return ActionResult::Unavailable;
  if (std::holds_alternative<Focus>(action)) {
    if (!node->isFocusable())
      return ActionResult::Unavailable;
    if (_table->focused == target)
      return ActionResult::Unchanged;
    requestFocus(target);
    return ActionResult::Applied;
  }
  if (std::holds_alternative<ScrollIntoView>(action)) {
    for (auto *p = node->parent(); p; p = p->parent())
      if (p->isPortal())
        break;
      else if (auto *scroll = dynamic_cast<ScrollView *>(p))
        scroll->scrollIntoView(*node);
    return ActionResult::Applied;
  }
  Traversal traversal{*_table};
  return node->performAction(action, source);
}

SemanticSnapshot UIRoot::semanticSnapshot() {
  Traversal traversal{*_table};
  navigationScope();
  if (auto *focused = resolve(_table->focused);
      focused && (!focused->isFocusable() || !acceptsAction(*focused)))
    requestFocus({});
  SemanticSnapshot result{.session = _workId, .focus = _table->focused};
  const auto label = [&](auto &&self, const Node &node) -> std::string {
    const auto s = node.semanticState();
    if (!s.description.name.empty())
      return s.description.name;
    if (s.description.role == SemanticRole::Text && s.description.value)
      return *s.description.value;
    std::string text;
    for (const auto &child : node.children()) {
      if (child->visibility() != Visibility::Visible ||
          child->semanticProps().exposure == SemanticExposure::HiddenSubtree)
        continue;
      auto part = self(self, *child);
      if (!part.empty()) {
        if (!text.empty())
          text += ' ';
        text += part;
      }
    }
    return text;
  };
  const auto visit = [&](auto &&self, const Node &node, NodeId parent,
                         bool enabled) -> void {
    auto state = node.semanticState();
    const auto exposure = state.description.exposure;
    if (node.visibility() != Visibility::Visible ||
        exposure == SemanticExposure::HiddenSubtree)
      return;
    enabled &= state.description.enabled;
    state.description.enabled = enabled;
    const auto role = state.description.role;
    bool expose = exposure == SemanticExposure::Self ||
                  (exposure != SemanticExposure::ChildrenOnly &&
                   (role != SemanticRole::None || node.isFocusable()));
    if (expose) {
      if (state.description.name.empty() && role != SemanticRole::Text)
        state.description.name = label(label, node);
      if (state.description.labelledBy)
        if (auto *other = resolve(*state.description.labelledBy))
          state.description.name = label(label, *other);
      if (state.description.describedBy)
        if (auto *other = resolve(*state.description.describedBy))
          state.description.description = label(label, *other);
      if (!enabled)
        state.customActions.clear();
      if (!enabled)
        state.actions.clear();
      for (auto &run : state.textRuns)
        run.bounds = node.worldTransform().mapBounds(run.bounds);
      result.nodes.push_back(
          {node.id(), parent,
           node.worldTransform().mapBounds({{}, node.bounds().size}),
           std::move(state)});
      parent = node.id();
    }
    if (exposure == SemanticExposure::Self)
      return;
    for (const auto &child : node.children()) {
      if (expose &&
          (role == SemanticRole::Button || role == SemanticRole::Checkbox ||
           role == SemanticRole::Switch) &&
          !child->isFocusable() && child->children().empty())
        continue;
      self(self, *child, parent, enabled);
    }
  };
  if (auto *modal = resolve(_modal))
    visit(visit, *modal, {}, true);
  else if (_content)
    visit(visit, *_content, {}, true);
  if (std::none_of(result.nodes.begin(), result.nodes.end(),
                   [&](const auto &n) { return n.id == result.focus; }))
    result.focus = {};
  return result;
}
} // namespace playground::ui
