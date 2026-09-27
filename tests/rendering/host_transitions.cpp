#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <app/HostTransitions.hpp>
#include <rendering/RenderBackendProps.hpp>
#include <runtime/ActivationLifetime.hpp>
#include <support/Test.hpp>

using namespace playground;

namespace {
struct App {
  int id;
  runtime::ActivationLifetime activation;

  explicit App(int value) : id{value} { activation.activate(); }
};

struct Painter : rendering::PaintContext {
  void save() override {}

  void restore() noexcept override {}

  void translate(math::Vec2f) override {}

  void clip(math::Rect) override {}

  void fill(math::Rect, math::ColorRGBA8) override {}
};

struct Backend : rendering::RenderBackend {
  std::vector<std::string> &events;
  rendering::RenderBackendProps props;
  bool failPresent{}, skip{};
  rendering::ResourceDomainId domain{rendering::acquireResourceDomain()};

  Backend(std::vector<std::string> &events, rendering::RenderBackendProps props)
      : events{events}, props{props} {}

  ~Backend() override { events.push_back("destroy backend"); }

  void invalidate() noexcept override { events.push_back("invalidate"); }

  rendering::RendererCandidate description() const override { return {}; }

  rendering::ResourceDomainId resourceDomain() const noexcept override {
    return domain;
  }

  math::Vec2i drawableSize() const override { return {8, 8}; }

  struct Frame : rendering::RenderFrame {
    Backend &backend;
    Painter painter;

    explicit Frame(Backend &backend) : backend{backend} {}

    ~Frame() override { backend.events.push_back("destroy frame"); }

    rendering::PaintContext &paint2D() override { return painter; }

    rendering::PresentationOutcome present() override {
      backend.events.push_back("present");
      if (backend.failPresent)
        throw rendering::RenderFailure{"test presentation",
                                       rendering::RenderOperation::Present};
      return rendering::PresentationOutcome::Submitted;
    }
  };

  std::unique_ptr<rendering::RenderFrame>
  beginFrame(rendering::RenderFrameProps) override {
    events.push_back("begin");
    return skip ? nullptr : std::make_unique<Frame>(*this);
  }
};
} // namespace

int main() {
  return test::run([] {
    std::unique_ptr<App> current = std::make_unique<App>(1);
    App *original = current.get();
    const auto originalActivation = current->activation.token();
    runtime::ActivationToken failedActivation;
    std::optional<int> pending{7};
    bool commandsSuppressed{};
    const auto request = [&](int command) {
      if (!commandsSuppressed)
        pending = command;
    };
    std::vector<int> cleaned;
    const auto cleanup = [&](App &value) noexcept {
      value.activation.deactivate();
      cleaned.push_back(value.id);
      app::suppressCommands(commandsSuppressed, [&] { request(99); });
    };
    test::rejects<std::runtime_error>(
        [&] {
          app::activateApp(
              current, std::make_unique<App>(2), [] {},
              [&] {
                failedActivation = current->activation.token();
                pending = 22;
                throw std::runtime_error("enter failed");
              },
              [&] { pending = 7; }, cleanup);
        },
        "failed activation propagates");
    test::require(current.get() == original && pending == 7 &&
                      cleaned == std::vector<int>{2},
                  "candidate cleaned, old app retained, commands restored");
    test::require(
        originalActivation.isActive() && !failedActivation.isActive(),
        "rollback preserves old work and invalidates failed candidate work");
    test::rejects<std::runtime_error>(
        [&] {
          app::activateApp(
              current, std::make_unique<App>(3),
              [] { throw std::runtime_error("capability preparation failed"); },
              [] {}, [] {}, cleanup);
        },
        "prepare failure does not enter candidate");
    test::require(current.get() == original && cleaned.size() == 1,
                  "unentered candidate gets no exit hook");
    app::activateApp(
        current, std::make_unique<App>(4), [] {}, [&] { pending = 44; }, [] {},
        cleanup);
    test::require(
        current->id == 4 && pending == 44 && cleaned.back() == 1,
        "successful switch cleans old app without leaking exit commands");
    test::require(!originalActivation.isActive() &&
                      current->activation.token().isActive(),
                  "successful switch invalidates old activation only");
    test::rejects<RestorationFailure>(
        [&] {
          app::activateApp(
              current, std::make_unique<App>(5), [] {},
              [] { throw std::runtime_error("enter"); },
              [] { throw std::runtime_error("rollback"); }, cleanup);
        },
        "failed host restoration is terminal");
    test::rejects<std::runtime_error>(
        [&] {
          app::suppressCommands(commandsSuppressed, [&] {
            request(99);
            throw std::runtime_error("exit");
          });
        },
        "throwing exit remains isolated");
    test::require(pending == 44 && !commandsSuppressed,
                  "throwing exit preserves previous command and restores gate");
    app::suppressCommands(commandsSuppressed, [&] {
      app::suppressCommands(commandsSuppressed, [] {});
      test::require(commandsSuppressed, "nested cleanup preserves suppression");
    });

    std::vector<std::string> events;
    events.reserve(64);
    rendering::RenderBackendProps props{
        .allocations = {.maxTargetBytes = 4096}};
    std::unique_ptr<rendering::RenderBackend> backend =
        std::make_unique<Backend>(events, props);
    static_cast<Backend &>(*backend).failPresent = true;
    test::rejects<rendering::RenderFailure>(
        [&] {
          app::renderFrame(
              *backend, {}, [&](auto &) { events.push_back("render"); },
              [&] { events.push_back("before present"); });
        },
        "native frame failure propagates after destruction");
    test::require(events.back() == "destroy frame",
                  "frame destroyed before recovery can start");
    runtime::UpdateClock clock;
    clock.advance(100, 1000);
    rendering::RecoveryState recovery;
    const auto oldDomain = backend->resourceDomain();
    app::recoverBackend(
        backend, recovery, clock, "present failed",
        [&] { events.push_back("release window"); },
        [&] {
          events.push_back("create");
          return std::make_unique<Backend>(events, props);
        },
        [&] {
          events.push_back("notify");
          test::require(backend->resourceDomain() != oldDomain,
                        "notify sees replacement domain");
        });
    test::require(events ==
                      std::vector<std::string>{
                          "begin", "render", "before present", "present",
                          "destroy frame", "invalidate", "destroy backend",
                          "release window", "create", "notify"},
                  "recovery order respects frame, window and backend lifetime");
    test::require(clock.advance(100000, 1000) == 0 &&
                      static_cast<Backend &>(*backend).props == props,
                  "recovery rebases clock and retains policy");
    static_cast<Backend &>(*backend).skip = true;
    bool rendered{};
    test::require(app::renderFrame(
                      *backend, {}, [&](auto &) { rendered = true; }, [] {}) ==
                          rendering::PresentationOutcome::Skipped &&
                      !rendered,
                  "unavailable frames do not call app render");
    test::rejects<rendering::RenderFailure>(
        [&] {
          app::recoverBackend(
              backend, recovery, clock, "again", [] {},
              [&] { return std::make_unique<Backend>(events, props); }, [] {});
        },
        "retry exhaustion does not replace backend");
    recovery = rendering::RecoveryState{};
    test::rejects<std::runtime_error>(
        [&] {
          app::recoverBackend(
              backend, recovery, clock, "factory failure", [] {},
              []() -> std::unique_ptr<rendering::RenderBackend> {
                throw std::runtime_error("cannot recreate device");
              },
              [] {});
        },
        "failed factory propagates");
    test::require(
        !backend && recovery.status() == rendering::RecoveryStatus::Exhausted,
        "failed recreation leaves no usable backend and exhausts recovery");
  });
}
