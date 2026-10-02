#pragma once

#include <ui/containers/Box.hpp>

namespace playground::ui {
enum class PresenceState { Entering, Present, Exiting, Hidden };

class Presence : public Box {
  bool _shown{true};
  std::uint64_t _generation{};
  PresenceState _state{PresenceState::Present};
  AnimationHandle _animation;
  void animate();

protected:
  void onAttach(UIServices &) override { animate(); }

  void onDetach() noexcept override { _animation.cancel(); }

public:
  explicit Presence(std::unique_ptr<Node> child, bool shown = true,
                    layout::BoxProps box = {});
  void setShown(bool);

  bool shown() const noexcept { return _shown; }

  PresenceState state() const noexcept { return _state; }
};

// At most two retained trees; incoming content alone determines preferred size.
class TransitionHost : public Node {
  ItemKey _key;
  AnimationHandle _enter, _exit;
  std::uint64_t _generation{};

protected:
  layout::MeasureResult
  measureContent(MeasureContext &, const layout::SizeConstraints &) override;
  void arrangeChildren(ArrangeContext &, math::Rect) override;

  void onDetach() noexcept override {
    _enter.cancel();
    _exit.cancel();
  }

public:
  explicit TransitionHost(layout::BoxProps box = {});
  void replace(ItemKey, std::unique_ptr<Node>, bool focusIncoming = false);

  const ItemKey &key() const noexcept { return _key; }

  Node *current() const noexcept {
    return children().empty() ? nullptr : children().back().get();
  }
};
} // namespace playground::ui
