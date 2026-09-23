#include <cmath>
#include <iostream>
#include <stdexcept>

#include <ui/UIRoot.hpp>
#include <ui/containers/Stack.hpp>

namespace math = playground::math;
namespace layout = playground::layout;
namespace ui = playground::ui;

static void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

class Probe : public ui::Node {
public:
  int measurements{};
  int events{};
  int detachments{};
  bool removeOnEvent{};
  bool invalidateDuringMeasure{};
  bool failArrange{};
  bool failPaint{};
  layout::SizeConstraints lastConstraints;
  ui::UIRoot *root{};
  explicit Probe(layout::BoxProps props = {}) : Node{std::move(props)} {
    setHitTestPolicy(ui::HitTestPolicy::Self);
    setFocusable(true);
  }

protected:
  layout::MeasureResult
  measureContent(ui::MeasureContext &,
                 const layout::SizeConstraints &constraints) override {
    ++measurements;
    lastConstraints = constraints;
    if (invalidateDuringMeasure)
      invalidateLayout();
    const float width = constraints.width.maximum.value_or(80);
    return {{width, width < 40 ? 40.0f : 20.0f}, 10.0f, 10.0f};
  }
  void onDetach() noexcept override { ++detachments; }
  void arrangeChildren(ui::ArrangeContext &, math::Rect) override {
    if (failArrange)
      throw std::runtime_error("arrangement failure");
  }
  void paint(ui::PaintContext &) const override {
    if (failPaint)
      throw std::runtime_error("paint failure");
  }
  void onEvent(ui::UIEvent &event) override {
    if (event.phase != ui::EventPhase::Target)
      return;
    if (event.type == ui::EventType::FocusGained)
      return;
    ++events;
    if (event.type == ui::EventType::PointerDown)
      capturePointer(event.pointer);
    if (removeOnEvent)
      root->defer([](ui::UIRoot &owner) { owner.setContent({}); });
  }
};

class TestContainer : public ui::Node {
protected:
  layout::MeasureResult
  measureContent(ui::MeasureContext &context,
                 const layout::SizeConstraints &offered) override {
    return children().empty() ? layout::MeasureResult{}
                              : children()[0]->measure(context, offered);
  }
  void arrangeChildren(ui::ArrangeContext &context,
                       math::Rect bounds) override {
    for (const auto &child : children())
      child->arrange(context, bounds);
  }

public:
  ui::Node &append(std::unique_ptr<ui::Node> child) {
    return appendChild(std::move(child));
  }
  std::unique_ptr<ui::Node> take() { return takeChildAt(0); }
};

class RecordingPaint : public ui::PaintContext {
public:
  int saves{}, fills{};
  void save() override { ++saves; }
  void restore() noexcept override { --saves; }
  void translate(math::Vec2f) override {}
  void clip(math::Rect) override {}
  void fill(math::Rect, math::ColorRGBA8) override { ++fills; }
};

class AttachMutator : public Probe {
  ui::UIRoot &_owner;

protected:
  void onAttach(ui::UIServices &) override { _owner.setContent({}); }

public:
  explicit AttachMutator(ui::UIRoot &owner) : _owner{owner} {}
};

int main() {
  try {
    ui::MeasureContext context;
    {
      ui::UIRoot owner;
      owner.setContent(std::make_unique<Probe>());
      auto *original = owner.content();
      bool rejected{};
      try {
        owner.setContent(std::make_unique<AttachMutator>(owner));
      } catch (const std::logic_error &) {
        rejected = true;
      }
      check(rejected && owner.content() == original,
            "attach callback cannot replace owning root");
    }
    Probe probe;
    layout::SizeConstraints constraints{
        layout::AxisConstraints::bounded(0, 100), {}};
    check(probe.measure(context, constraints).size == math::Size2{100, 20},
          "initial measure");
    probe.measure(context, constraints);
    check(probe.measurements == 1, "unchanged measure must be cached");
    constraints.width.maximum = 30.0f;
    check(probe.measure(context, constraints).size.height == 40,
          "changed width remeasures wrapping");
    auto props = probe.boxProps();
    props.maxWidth = 20.0f;
    probe.setBoxProps(props);
    constraints.width.maximum = 100.0f;
    check(probe.measure(context, constraints).size == math::Size2{20, 40},
          "authored maximum constrains content");
    constraints.width = layout::AxisConstraints::tight(100);
    check(probe.measure(context, constraints).size == math::Size2{100, 20},
          "parent minimum overrides incompatible authored maximum");
    check(probe.lastConstraints.width.minimum == 100 &&
              probe.lastConstraints.width.maximum == 100,
          "content measurement must use the same effective width as final box");
    Probe minimumProbe{{.minWidth = 50}};
    minimumProbe.measure(context, {});
    check(minimumProbe.lastConstraints.width.minimum == 50,
          "authored minimum reaches content measurement");
    probe.invalidateDuringMeasure = true;
    probe.invalidateLayout();
    probe.measure(context, constraints);
    const auto previousMeasurements = probe.measurements;
    probe.measure(context, constraints);
    check(probe.measurements == previousMeasurements + 1,
          "invalidation during measurement must not cache stale output");
    probe.failArrange = true;
    try {
      probe.arrange(context, math::rect(0, 0, 100, 100));
    } catch (const std::runtime_error &) {
    }
    check(!probe.isArranged(),
          "failed arrangement must not mark bounds ready for input");
    {
      ui::UIRoot root;
      bool flushed{};
      root.defer([&](ui::UIRoot &) { flushed = true; });
      ui::UIEvent event{.type = ui::EventType::PointerMove};
      root.dispatch(event);
      check(flushed, "empty-root dispatch flushes deferred mutations");
      bool recursiveRejected{}, secondRan{};
      root.defer([&](ui::UIRoot &owner) {
        try {
          owner.flushMutations();
        } catch (const std::logic_error &) {
          recursiveRejected = true;
        }
      });
      root.defer([&](ui::UIRoot &) { secondRan = true; });
      root.flushMutations();
      check(recursiveRejected && secondRan,
            "recursive mutation flush cannot drain outer queue");
      auto container = std::make_unique<TestContainer>();
      auto *owner = container.get();
      auto child = std::make_unique<Probe>();
      auto *pointer = child.get();
      container->append(std::move(child));
      root.setContent(std::move(container));
      root.flushLayout({100, 80});
      pointer->requestFocus();
      ui::UIEvent down{.type = ui::EventType::PointerDown,
                       .position = {10, 10},
                       .pointer = 9};
      root.dispatch(down);
      owner->setVisibility(ui::Visibility::Hidden);
      ui::UIEvent key{.type = ui::EventType::KeyDown};
      root.dispatch(key);
      check(pointer->events == 2,
            "hidden ancestor cancels capture and prevents key delivery");
      owner->setVisibility(ui::Visibility::Visible);
      pointer->requestFocus();
      pointer->setFocusable(false);
      root.dispatch(key);
      check(pointer->events == 2,
            "nonfocusable node cannot retain keyboard focus");
      root.dispatch(down);
      owner->setHitTestPolicy(ui::HitTestPolicy::None);
      root.dispatch(key);
      check(pointer->events == 4, "disabled hit-test subtree cancels capture");
      owner->setHitTestPolicy(ui::HitTestPolicy::ChildrenOnly);
      pointer->failPaint = true;
      RecordingPaint paint;
      try {
        root.render(paint);
      } catch (const std::runtime_error &) {
      }
      check(paint.saves == 0, "paint exceptions restore all scopes");
      pointer->failPaint = false;
      flushed = false;
      root.defer([&](ui::UIRoot &) { flushed = true; });
      ui::UIEvent outside{.type = ui::EventType::PointerMove,
                          .position = {200, 200}};
      root.dispatch(outside);
      check(flushed, "no-hit dispatch flushes deferred mutations");
      pointer->removeOnEvent = true;
      pointer->root = &root;
      root.dispatch(down);
      check(!root.content(), "event deferred removal flushes");
      auto focused = std::make_unique<Probe>();
      auto *focusPointer = focused.get();
      root.setContent(std::move(focused));
      root.flushLayout({100, 80});
      focusPointer->requestFocus();
      focusPointer->root = &root;
      focusPointer->removeOnEvent = true;
      ui::UIEvent lost{.type = ui::EventType::FocusLost};
      root.dispatch(lost);
      check(!root.content(), "focus-lost callbacks flush deferred work");
    }
    ui::NodeHandle<Probe> surviving;
    {
      ui::UIRoot root;
      auto container = std::make_unique<TestContainer>();
      auto *owner = container.get();
      auto child = std::make_unique<Probe>();
      auto *pointer = child.get();
      container->append(std::move(child));
      root.setContent(std::move(container));
      surviving = pointer->handle<Probe>();
      check(surviving.get() == pointer, "attached node resolves");
      root.flushLayout({100, 80});
      pointer->setBackground(math::ColorRGBA8{1, 2, 3, 255});
      RecordingPaint paint;
      root.render(paint);
      check(paint.saves == 0 && paint.fills == 1, "paint state balanced");
      ui::UIEvent down{.type = ui::EventType::PointerDown,
                       .position = {10, 10},
                       .pointer = 7,
                       .button = 1};
      root.dispatch(down);
      ui::UIEvent outside{.type = ui::EventType::PointerUp,
                          .position = {200, 200},
                          .pointer = 7,
                          .button = 1};
      root.dispatch(outside);
      check(pointer->events == 2, "capture routes release outside bounds");
      auto detached = owner->take();
      check(!surviving && pointer->detachments == 1,
            "detachment expires handle and calls hook");
      owner->append(std::move(detached));
      surviving = pointer->handle<Probe>();
      check(surviving.get() == pointer,
            "reattachment gets a new valid identity");
      pointer->removeOnEvent = true;
      pointer->root = &root;
      root.flushLayout({100, 80});
      root.dispatch(down);
      check(root.content() == nullptr && !surviving,
            "self-removal deferred until dispatch ends");
      auto last = std::make_unique<Probe>();
      auto *lastPointer = last.get();
      root.setContent(std::move(last));
      surviving = lastPointer->handle<Probe>();
    }
    check(!surviving, "root destruction expires handles");
    {
      ui::Scheduler scheduler;
      int calls{};
      auto timer = scheduler.schedule(1, [&] { ++calls; }, 0.5);
      scheduler.advance(0.5);
      check(calls == 0, "timer not early");
      scheduler.advance(0.5);
      check(calls == 1, "timer deadline");
      scheduler.advance(20);
      check(calls == 2, "timer avoids unbounded catch-up");
      timer.disconnect();
      scheduler.advance(1);
      check(calls == 2, "timer RAII cancellation");
      ui::Signal<> signal;
      auto subscription = signal.connect([&] { ++calls; });
      signal.emit();
      subscription.disconnect();
      signal.emit();
      check(calls == 3, "subscription disconnect");
      ui::Connection replacement{std::move(subscription)};
      subscription.disconnect();
      replacement.disconnect();
      auto tiny = scheduler.schedule(
          0, [&] { ++calls; }, std::numeric_limits<double>::denorm_min());
      scheduler.advance(0);
      check(calls == 4, "sub-ULP repeating interval cannot spin forever");
    }
    {
      ui::UIRoot root;
      auto owner = std::make_unique<ui::HStack>();
      auto from = std::make_unique<ui::VStack>();
      auto *fromPtr = from.get();
      auto to = std::make_unique<ui::VStack>();
      auto *toPtr = to.get();
      auto *child = &from->append(std::make_unique<Probe>());
      owner->append(std::move(from));
      owner->append(std::move(to));
      root.setContent(std::move(owner));
      root.flushLayout({200, 100});
      const auto handle = child->handle();
      root.reparent(child->id(), *fromPtr, *toPtr, layout::StackPlacement{});
      check(handle && handle.get() == child && child->parent() == toPtr,
            "same-root reparent preserves identity");
      root.flushChanges();
      int changes{};
      auto watch = child->onChanged([&](const ui::ChangeSet &) { ++changes; });
      child->setBackground(math::ColorRGBA8{1, 2, 3, 255});
      child->setBackground(math::ColorRGBA8{4, 5, 6, 255});
      root.flushChanges();
      check(changes == 1, "property notifications coalesced");
      const auto revision = child->sourceRevision();
      auto sink = root.completionSink();
      bool completed{};
      sink.post(handle, revision, [&](ui::Node &) { completed = true; });
      child->invalidatePaint();
      root.update(0);
      check(!completed, "stale worker result discarded");
      sink.post(handle, child->sourceRevision(),
                [&](ui::Node &) { completed = true; });
      root.update(0);
      check(completed, "current worker result applied");
      root.flushLayout({200, 100});
      const auto measured = root.stats().measured;
      root.flushLayout({200, 100});
      check(root.stats().measured == measured, "clean root skips layout");
    }
    std::cout << "UI runtime tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
