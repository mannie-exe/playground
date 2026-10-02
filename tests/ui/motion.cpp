#include <atomic>
#include <chrono>
#include <thread>

#include <support/Test.hpp>
#include <ui/Async.hpp>
#include <ui/UIRoot.hpp>
#include <ui/content/Rectangle.hpp>
#include <ui/controls/Button.hpp>

using namespace playground;
using namespace playground::ui;

namespace {
void close(float a, float b, const char *message) {
  test::require(std::abs(a - b) < .001f, message);
}

std::unique_ptr<Node> box() { return std::make_unique<Box>(); }
} // namespace

int main() {
  int result = test::run([] {
    UIServices themedServices;
    themedServices.theme.motion.feedback.duration = 2;
    UIRoot themedRoot{themedServices};
    MotionValue<float> themedValue{0};
    auto themedAnimation = themedRoot.motion().transition(
        themedValue.binding(), 1.f, MotionRole::Feedback);
    themedRoot.update(1);
    themedRoot.motion().sample();
    close(themedValue.value(), .5f,
          "constructor theme supplies motion role timing");
    MotionEngine engine;
    MotionValue<float> value{0};
    unsigned notifications = 0;
    auto animation = engine.transition(
        value.binding(), 10.f, {.duration = 1}, [&](auto outcome) {
          test::require(outcome == AnimationStatus::Completed,
                        "completion outcome");
          ++notifications;
        });
    test::require(value.authored() == 10 && value.value() == 0,
                  "transition authors target separately");
    engine.advance(.25);
    close(value.value(), 0,
          "clock advance does not sample intermediate properties");
    engine.sample();
    close(value.value(), 2.5, "linear quarter sample");
    animation.pause();
    engine.advance(10);
    engine.sample();
    close(value.value(), 2.5, "paused time freezes");
    test::require(!engine.needsFrame() && !engine.nextDelay(),
                  "paused animation is idle");
    animation.resume();
    animation.seek(.75);
    close(value.value(), 7.5, "seek samples");
    engine.advance(5);
    close(value.value(), 10, "large delta completes");
    engine.dispatchCompletions();
    engine.dispatchCompletions();
    test::require(notifications == 1 &&
                      animation.status() == AnimationStatus::Completed &&
                      engine.stats().tracks == 0,
                  "completion delivered once and storage retired");
    auto first = engine.transition(value.binding(), 20.f, {.duration = 1});
    engine.advance(.5);
    engine.sample();
    close(value.value(), 15, "retarget initial trajectory");
    auto second = engine.transition(value.binding(), 0.f, {.duration = 1});
    test::require(first.status() == AnimationStatus::Replaced,
                  "exclusive property replacement");
    close(value.value(), 15, "replacement starts at displayed value");
    second.cancel();
    close(value.value(), 0, "cancel reveals authored destination");
    auto effect = engine.play(value.binding(),
                              Keyframes<float>{{{0, 5}, {.5, 15}, {1, 5}}},
                              {.duration = 1});
    engine.advance(.5);
    engine.sample();
    close(value.value(), 15, "typed intermediate keyframe");
    effect.finish();
    close(value.value(), 0, "keyframes restore authored state");
    auto overwritten = engine.transition(value.binding(), 9.f, {.duration = 1});
    value.set(3);
    engine.advance(0);
    engine.sample();
    close(value.value(), 3, "direct writes win");
    test::require(overwritten.status() == AnimationStatus::Replaced,
                  "direct setter outcome");
    auto delayed =
        engine.transition(value.binding(), 8.f, {.duration = 1, .delay = 2});
    test::require(!engine.needsFrame() && engine.nextDelay() == 2,
                  "delay sleeps until start");
    engine.dispatchCompletions();
    test::require(engine.nextDelay() == 2, "delayed start deadline");
    engine.advance(2);
    test::require(engine.needsFrame(), "start deadline creates frame demand");
    delayed.cancel();
    engine.dispatchCompletions();
    auto reverse = engine.play(
        value.binding(), Keyframes<float>{{{0, 0}, {1, 10}}},
        {.duration = 1, .repeats = 1, .direction = MotionDirection::Alternate});
    engine.advance(1.25);
    engine.sample();
    close(value.value(), 7.5, "alternate iteration");
    reverse.cancel();
    engine.dispatchCompletions();
    test::require(Easing{.kind = Easing::Kind::Steps, .steps = 4}.sample(.49) ==
                      .25,
                  "step easing");
    close(static_cast<float>(Easing{.kind = Easing::Kind::CubicBezier,
                                    .x1 = .42,
                                    .y1 = 0,
                                    .x2 = .58,
                                    .y2 = 1}
                                 .sample(.5)),
          .5, "bezier solves x before y");
    test::rejects<std::invalid_argument>(
        [] { Keyframes<float>{{{0, 0}, {0, 2}, {1, 3}}}.validate(); },
        "duplicate offsets rejected");
    test::rejects<std::invalid_argument>([&] { animation.seek(-1); },
                                         "negative seek rejected");
    MotionBindings bindings;
    bindings.add(value.binding());
    TimelineSpec timeline;
    timeline.at(0, 0, Keyframes<float>{{{0, 0}, {1, 1}}});
    timeline.at(0, 0, Keyframes<float>{{{0, 0}, {1, 1}}});
    test::rejects<std::invalid_argument>(
        [&] { auto invalid = engine.play(timeline, bindings); },
        "conflicting timeline rejected");
    TimelineSpec sequence;
    sequence.at(1, 0, Keyframes<float>{{{0, 10}, {1, 20}}}, {.duration = 1});
    sequence.at(0, 0, Keyframes<float>{{{0, 0}, {1, 10}}}, {.duration = 1});
    auto sequential = engine.play(sequence, bindings);
    engine.advance(1.5);
    engine.sample();
    close(value.value(), 15, "offset ordering independent of insertion");
    sequential.cancel();
    engine.dispatchCompletions();
    auto hold =
        engine.transition(value.binding(), 20.f, {.duration = 1, .delay = 1});
    const auto held = value.value();
    engine.advance(.5);
    engine.sample();
    close(value.value(), held, "delayed transition retains displayed origin");
    hold.seek(1.5);
    hold.seek(.25);
    close(value.value(), held, "seeking into delay restores origin");
    hold.cancel();
    engine.dispatchCompletions();
    MotionValue<float> callbackValue{0};
    unsigned callbacks = 0;
    auto throws =
        engine.transition(value.binding(), 30.f, {.duration = 0}, [](auto) {
          throw std::runtime_error("callback failure");
        });
    auto survives =
        engine.transition(callbackValue.binding(), 30.f, {.duration = 0},
                          [&](auto) { ++callbacks; });
    test::rejects<std::runtime_error>([&] { engine.dispatchCompletions(); },
                                      "callback exception propagated");
    engine.dispatchCompletions();
    test::require(callbacks == 1,
                  "other completion survives callback exception");
    MotionEngine bounded{{1, 2}};
    MotionValue<float> other{0};
    auto admitted = bounded.transition(value.binding(), 1.f);
    test::rejects<std::length_error>(
        [&] { auto rejected = bounded.transition(other.binding(), 1.f); },
        "track budget refuses admission");
    test::require(bounded.stats().tracks == 1 &&
                      bounded.stats().keyframes == 2 &&
                      bounded.stats().retainedBytes > 0,
                  "known storage accounting");
    auto retargeted = bounded.transition(value.binding(), 2.f);
    test::require(admitted.status() == AnimationStatus::Replaced &&
                      bounded.stats().tracks == 1,
                  "replacement reuses bounded admission");
    retargeted.cancel();
    engine.setPreference(MotionPreference::None);
    auto instant = engine.transition(other.binding(), 5.f);
    test::require(instant.status() == AnimationStatus::Completed &&
                      other.value() == 5 && !engine.needsFrame(),
                  "no-motion settles immediately");
    engine.setPreference(MotionPreference::Full);
    auto running = engine.transition(other.binding(), 8.f, {.duration = 1});
    engine.setPreference(MotionPreference::System, true);
    test::require(running.status() == AnimationStatus::Completed &&
                      other.value() == 8,
                  "live reduced preference settles playback");
    auto spatial =
        engine.transition(other.binding(true), 12.f, {.duration = 1});
    test::require(spatial.status() == AnimationStatus::Completed,
                  "reduced spatial motion settles");
    engine.setPreference(MotionPreference::Full, true);
    auto forcedFull =
        engine.transition(other.binding(true), 14.f, {.duration = 1});
    test::require(forcedFull.status() == AnimationStatus::Running,
                  "explicit full motion overrides native reduced preference");
    engine.setPreference(MotionPreference::System, false);
    test::require(forcedFull.status() == AnimationStatus::Running,
                  "returning to system full preserves live playback");
    engine.setPreference(MotionPreference::System, true);
    test::require(forcedFull.status() == AnimationStatus::Completed,
                  "native reduced change settles existing playback");
    engine.dispatchCompletions();

    MotionEngine guarded;
    MotionValue<float> reentrant{0, [&] { guarded.sample(); }};
    test::rejects<std::logic_error>(
        [&] { auto invalid = guarded.transition(reentrant.binding(), 1.f); },
        "recursive sampling rejected safely");
    test::require(reentrant.value() == reentrant.authored(),
                  "failed playback removes temporary effect");
    UIRoot root;
    auto button = std::make_unique<Button>();
    auto *target = button.get();
    root.setContent(std::move(button));
    root.flushLayout({100, 100});
    auto opacity = root.motion().transition(motion::opacity(target->handle()),
                                            0.f, {.duration = 1});
    root.update(.5);
    root.motion().sample();
    close(std::get<float>(target->motionValue(MotionProperty::Opacity)), .5,
          "node presentation override");
    target->setBackground(math::ColorRGBA8{100, 100, 100, 255});
    root.update(0);
    test::require(opacity.status() == AnimationStatus::Running,
                  "unrelated background write preserves opacity owner");
    target->applyPaintPatch({.opacity = Patch<float>::set(0.f)});
    root.update(0);
    test::require(opacity.status() == AnimationStatus::Replaced,
                  "same-value property patch replaces effect");
    target->setMotionValue(MotionProperty::Opacity, 1.f);
    opacity = root.motion().transition(motion::opacity(target->handle()), 0.f,
                                       {.duration = 1});
    root.update(.5);
    root.motion().sample();
    auto style = target->paintStyle();
    target->setPaintStyle(style);
    root.update(0);
    test::require(opacity.status() == AnimationStatus::Replaced,
                  "same-value explicit paint setter replaces effect");
    target->setMotionValue(MotionProperty::Opacity, 1.f);
    auto movement =
        root.motion().transition(motion::translation(target->handle()),
                                 math::Vec2f{100, 0}, {.duration = 1});
    root.update(.5);
    root.motion().sample();
    test::require(root.hitTest({75, 50}).has_value() &&
                      !root.hitTest({25, 50}).has_value(),
                  "hit testing follows sampled transform");
    UIEvent press{.type = EventType::PointerDown,
                  .position = {75, 50},
                  .pointer = 1,
                  .button = 1};
    root.dispatch(press);
    test::require(target->isPressed(), "gesture active before inert exit");
    root.requestFocus(target->id());
    target->setInert(true);
    test::require(!target->isPressed() && root.focusedNode() == NodeId{},
                  "inert exit cancels capture and focus");
    test::require(!root.hitTest({75, 50}) &&
                      root.semanticSnapshot().nodes.empty(),
                  "inert trees excluded from input and semantics");
    root.setContent(box());
    root.update(0);
    test::require(movement.status() == AnimationStatus::TargetGone,
                  "detached node outcome");

    AnimationHandle timerMotion;
    auto scheduled = root.services().scheduler->schedule(1, [&] {
      timerMotion = root.motion().transition(
          motion::opacity(root.content()->handle()), 0.f, {.duration = 1});
    });
    root.update(10);
    test::require(timerMotion.status() == AnimationStatus::Running,
                  "timer-started animation does not inherit old elapsed time");
    timerMotion.cancel();
    auto presence = std::make_unique<Presence>(box());
    auto *visible = presence.get();
    root.setContent(std::move(presence));
    root.update(1);
    visible->setShown(false);
    root.update(.05);
    root.motion().sample();
    test::require(visible->state() == PresenceState::Exiting &&
                      visible->isInert(),
                  "exit retains inert content");
    visible->setShown(true);
    root.update(1);
    test::require(visible->state() == PresenceState::Present &&
                      visible->visibility() == Visibility::Visible &&
                      !visible->isInert(),
                  "presence reversal");
    visible->setShown(false);
    root.update(1);
    test::require(visible->state() == PresenceState::Hidden &&
                      visible->visibility() == Visibility::Collapsed,
                  "exit collapses");
    auto host = std::make_unique<TransitionHost>();
    auto *transitionHost = host.get();
    root.setContent(std::move(host));
    transitionHost->replace("first", box());
    transitionHost->replace("second", box());
    transitionHost->replace("third", box());
    test::require(transitionHost->children().size() == 2 &&
                      transitionHost->children().front()->isInert(),
                  "rapid replacements retain at most two trees");
    root.update(1);
    root.update(0);
    test::require(transitionHost->children().size() == 1 &&
                      transitionHost->key() == "third",
                  "outgoing tree retired");

    runtime::Executor executor{{.workers = 1, .maxOutstanding = 2}};
    auto resource = std::make_shared<AsyncResource<int>>();
    std::atomic<bool> release{};
    resource->start(executor, [&](std::stop_token) {
      while (!release.load())
        std::this_thread::yield();
      return 1;
    });
    resource->start(executor, [](std::stop_token) { return 2; });
    release = true;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (resource->snapshot().status == AsyncStatus::Pending &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    test::require(resource->snapshot().status == AsyncStatus::Ready &&
                      *resource->snapshot().value == 2,
                  "superseded worker cannot publish stale result");
    auto view = std::make_unique<AsyncView<int>>(
        resource,
        AsyncViewProps<int>{.pending = [] { return box(); },
                            .ready = [](int) { return box(); },
                            .error = [](const auto &) { return box(); }});
    auto *async = view.get();
    root.setContent(std::move(view));
    root.update(0);
    test::require(async->current() != nullptr,
                  "async view publishes ready content on owner");
    auto *retained = async->current();
    std::atomic<bool> refreshDone{};
    resource->start(executor, [&](std::stop_token stop) {
      while (!refreshDone.load() && !stop.stop_requested())
        std::this_thread::yield();
      return 2;
    });
    async->refresh();
    const bool sameContent = async->current() == retained;
    refreshDone = true;
    test::require(sameContent,
                  "refresh preserves previous successful subtree identity");
    resource->cancel();
    const auto retiredDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (executor.stats().outstanding &&
           std::chrono::steady_clock::now() < retiredDeadline)
      std::this_thread::yield();
    resource->start(executor, [](std::stop_token) -> int {
      throw std::runtime_error("expected error");
    });
    async->refresh();
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (resource->snapshot().status == AsyncStatus::Pending &&
           std::chrono::steady_clock::now() < end)
      std::this_thread::yield();
    root.update(.02);
    test::require(resource->snapshot().status == AsyncStatus::Error &&
                      resource->snapshot().error == "expected error" &&
                      *resource->snapshot().value == 2,
                  "errors retain previous success");
    auto pendingResource = std::make_shared<AsyncResource<int>>();
    std::atomic<bool> releasePending{};
    pendingResource->start(
        executor,
        [&](std::stop_token stop) {
          while (!releasePending && !stop.stop_requested())
            std::this_thread::yield();
          return 7;
        },
        0, [] { throw std::runtime_error("wake unavailable"); });
    unsigned pendingViews = 0, readyViews = 0;
    auto pendingView = std::make_unique<AsyncView<int>>(
        pendingResource, AsyncViewProps<int>{.pending =
                                                 [&] {
                                                   ++pendingViews;
                                                   return box();
                                                 },
                                             .ready =
                                                 [&](int) {
                                                   ++readyViews;
                                                   return box();
                                                 }});
    root.setContent(std::move(pendingView));
    root.update(0);
    root.update(.1);
    test::require(pendingViews == 0, "initial fallback waits before showing");
    root.update(.051);
    test::require(pendingViews == 1, "pending fallback appears after delay");
    releasePending = true;
    const auto readyDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (pendingResource->snapshot().status == AsyncStatus::Pending &&
           std::chrono::steady_clock::now() < readyDeadline)
      std::this_thread::yield();
    root.update(.016);
    test::require(
        readyViews == 1 &&
            pendingResource->snapshot().status == AsyncStatus::Ready,
        "durable result survives wake failure and reveals immediately");
    root.update(1);
    test::require(!root.nextUpdateDelay(),
                  "settled async view stops observation timer");
    executor.close();
    resource->start(executor, [](std::stop_token) { return 3; });
    test::require(resource->snapshot().status == AsyncStatus::Error,
                  "executor refusal observable");
  });
  result |= test::run([] {
    for (unsigned available : {0u, 2u}) {
      UIRoot root;
      auto host = std::make_unique<TransitionHost>();
      auto *view = host.get();
      root.setContent(std::move(host));
      view->replace("original", std::make_unique<Button>());
      root.update(1);
      auto *original = view->current();
      root.flushLayout({100, 100});
      root.requestFocus(original->id());
      MotionValue<float> occupied{0};
      Keyframes<float> frames;
      const unsigned count = 16384 - available;
      for (unsigned i = 0; i < count; ++i)
        frames.values.push_back({double(i) / (count - 1), float(i % 2)});
      auto capacity =
          root.motion().play(occupied.binding(), frames, {.duration = 100});
      test::rejects<std::length_error>(
          [&] { view->replace("candidate", box()); },
          "replacement exercises animation admission failure");
      test::require(
          view->current() == original && view->key() == "original" &&
              view->children().size() == 1 && !original->isInert(),
          "animation refusal restores previous usable content and key");
      root.update(0);
      test::require(original->hasFocus(),
                    "replacement rollback restores scheduled focus");
    }
  });
  result |= test::run([] {
    runtime::Executor executor{{.workers = 1, .maxOutstanding = 2}};
    std::atomic<bool> running{};
    auto blocker = executor.submit(
        [&](std::stop_token stop) noexcept {
          running = true;
          while (!stop.stop_requested())
            std::this_thread::yield();
        },
        0);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!running && std::chrono::steady_clock::now() < deadline)
      std::this_thread::yield();
    test::require(running, "worker occupied before queueing request");
    auto resource = std::make_shared<AsyncResource<int>>();
    std::atomic<bool> invoked{};
    resource->start(executor, [&](std::stop_token) {
      invoked = true;
      return 1;
    });
    UIRoot root;
    root.setContent(std::make_unique<AsyncView<int>>(
        resource,
        AsyncViewProps<int>{.ready = [](int) { return box(); },
                            .error = [](const auto &) { return box(); }}));
    root.update(0);
    executor.close();
    root.update(.02);
    root.update(1);
    test::require(!invoked &&
                      resource->snapshot().status == AsyncStatus::Cancelled &&
                      !root.nextUpdateDelay(),
                  "discarded queued request cancels and stops view polling");
  });
  result |= test::run([] {
    MotionEngine engine;
    MotionValue<float> opacity{0}, translation{0};
    MotionBindings bindings;
    bindings.add(opacity.binding());
    bindings.add(translation.binding());
    TimelineSpec spec;
    spec.at(0, 0, Keyframes<float>{{{0, 0}, {1, 10}}}, {.duration = 1});
    spec.at(0, 1, Keyframes<float>{{{0, 0}, {1, 10}}}, {.duration = 1});
    AnimationStatus outcome = AnimationStatus::Running;
    auto both =
        engine.play(spec, bindings, [&](auto status) { outcome = status; });
    engine.advance(.25);
    engine.sample();
    auto replacement =
        engine.transition(opacity.binding(), 20.f, {.duration = 1});
    close(translation.value(), 2.5f,
          "partial replacement preserves unrelated presented property");
    engine.advance(.25);
    engine.sample();
    close(translation.value(), 5.f,
          "unrelated track continues on original clock");
    test::require(both.status() == AnimationStatus::Running,
                  "partially replaced timeline remains active");
    engine.advance(.5);
    engine.dispatchCompletions();
    test::require(both.status() == AnimationStatus::Replaced &&
                      outcome == AnimationStatus::Replaced,
                  "partial replacement reports interrupted completion after "
                  "survivors finish");
  });
  return result;
}
