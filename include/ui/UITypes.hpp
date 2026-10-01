#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <math/Geometry2D.hpp>
#include <ui/NodeProps.hpp>
#include <ui/RuntimeServices.hpp>
#include <ui/Semantics.hpp>
#include <ui/WorkDiagnostics.hpp>

namespace playground::ui {

class Node;

enum class EventType {
  PointerMove,
  PointerDown,
  PointerUp,
  PointerCancel,
  PointerEnter,
  PointerLeave,
  Wheel,
  KeyDown,
  KeyUp,
  FocusLost,
  FocusGained,
  FocusWithinGained,
  FocusWithinLost,
  TextInput,
  TextEditing,
  InputCancel
};
enum class EventPhase { Capture, Target, Bubble };
enum class Key {
  Unknown,
  Space,
  Enter,
  Tab,
  Escape,
  Left,
  Right,
  Up,
  Down,
  Home,
  PageUp,
  PageDown,
  ContextMenu,
  End,
  Backspace,
  Delete,
  A,
  C,
  V,
  X,
  Y,
  Z
};

struct UIEvent {
  EventType type{};
  EventPhase phase{EventPhase::Target};
  math::Point2 position{};
  math::Point2 localPosition{};
  math::Vec2f delta{};

  std::uint64_t pointer{};
  int button{};
  int key{};
  Key logicalKey{Key::Unknown};
  bool repeat{};
  bool shift{};
  bool control{}, alt{}, command{};
  std::string text;
  int compositionStart{}, compositionLength{};
  ActionSource source{ActionSource::Keyboard};

  bool handled{};
  bool propagationStopped{};
  bool defaultPrevented{};

  void stopPropagation() noexcept { propagationStopped = true; }

  void preventDefault() noexcept { defaultPrevented = true; }
};

enum class LayoutPhase { Measure, Arrange, Prepare, Paint, Input };
enum class LayoutIssue {
  UnboundedFill,
  IndefinitePercent,
  ConstraintViolation,
  AspectConflict,
  FontDoesNotFit,
  NonInvertibleTransform,
  IndefiniteAnchor
};

struct LayoutDiagnostic {
  NodeId node;
  LayoutPhase phase;
  LayoutIssue issue;
  std::string message;
};

class LayoutDiagnostics {
  std::vector<LayoutDiagnostic> _entries;

public:
  void report(NodeId node, LayoutPhase phase, LayoutIssue issue,
              std::string message) {
    if (_entries.size() >= 256)
      _entries.erase(_entries.begin());
    _entries.push_back({node, phase, issue, std::move(message)});
  }

  const std::vector<LayoutDiagnostic> &entries() const noexcept {
    return _entries;
  }

  void clear() noexcept { _entries.clear(); }
};

namespace detail {
struct NodeTable {
  struct Slot {
    Node *node{};
    std::uint64_t generation{1};
    std::uint32_t nextFree{std::numeric_limits<std::uint32_t>::max()};
  };

  struct Capture {
    std::uint64_t pointer;
    NodeId node;
  };

  std::vector<Slot> slots;
  std::uint32_t freeHead{std::numeric_limits<std::uint32_t>::max()};
  std::size_t traversals{};
  std::size_t lifecycleCallbacks{};

  UIServices *services{};
  UIWorkStats *stats{};
  std::vector<NodeId> dirty;
  std::vector<NodeId> layoutBoundaries;
  bool dirtyFallback{};
  bool layoutDirty{true};
  bool paintDirty{true};
  std::uint64_t revision{1};
  std::uint64_t geometryRevision{1};

  NodeId focused{};
  std::function<void(NodeId)> requestFocus;
  std::vector<Capture> captures;

  Node *resolve(NodeId id) const noexcept {
    if (id.index >= slots.size())
      return nullptr;
    const auto &slot = slots[id.index];
    return slot.generation == id.generation ? slot.node : nullptr;
  }
};
} // namespace detail

template <typename T = Node> class NodeHandle {
  std::weak_ptr<detail::NodeTable> _table;
  NodeId _id;

public:
  NodeHandle() = default;

  NodeHandle(std::weak_ptr<detail::NodeTable> table, NodeId id)
      : _table{std::move(table)}, _id{id} {}

  T *get() const noexcept {
    auto table = _table.lock();
    return table ? dynamic_cast<T *>(table->resolve(_id)) : nullptr;
  }

  explicit operator bool() const noexcept { return get() != nullptr; }

  NodeId id() const noexcept { return _id; }
};

struct HitResult {
  NodeHandle<> target;
  math::Point2 localPosition;
  std::vector<NodeId> path;
};

} // namespace playground::ui
