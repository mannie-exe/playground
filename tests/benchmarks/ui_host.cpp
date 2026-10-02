#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

#include <app/AppHost.hpp>
#include <support/TemporaryDirectory.hpp>
#include <support/Test.hpp>
#include <ui/collections/ScrollView.hpp>

using namespace playground;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {
constexpr int moves = 80;

auto ticks() { return Clock::now().time_since_epoch().count(); }

struct Geometry {
  float x{}, y{}, travel{};
};

struct Samples {
  Geometry geometry;
  std::array<std::atomic<Clock::rep>, moves> sent{};
  std::vector<double> dispatch, paint;
  int dispatched{-1}, painted{-1};
  std::promise<void> completed;
};

class ObservedSettings final : public ui::SettingsView {
  Samples &_samples;

protected:
  void onEvent(ui::UIEvent &event) override {
    if (event.phase != ui::EventPhase::Capture ||
        event.type != ui::EventType::PointerMove || !_samples.geometry.travel)
      return;
    const auto index = int(std::round((event.position.y - _samples.geometry.y) /
                                      _samples.geometry.travel * moves)) -
                       1;
    if (index < 0 || index >= moves)
      return;
    const auto sent = _samples.sent[index].load();
    if (!sent)
      return;
    _samples.dispatched = index;
    _samples.dispatch.push_back(std::chrono::duration<double, std::milli>{
        Clock::duration{ticks() -
                        sent}}.count());
  }

  void paintSubtree(ui::PaintContext &context) const override {
    ui::SettingsView::paintSubtree(context);
    const auto index = _samples.dispatched;
    if (index >= 0 && index != _samples.painted) {
      _samples.painted = index;
      _samples.paint.push_back(std::chrono::duration<double, std::milli>{
          Clock::duration{ticks() - _samples.sent[index].load()}}
                                   .count());
      if (index == moves - 1)
        _samples.completed.set_value();
    }
  }

public:
  ObservedSettings(AssetRegistry &assets, FontHandle font,
                   rendering::GraphicsSettings settings,
                   ui::SettingsViewActions actions, Samples &samples)
      : SettingsView{assets, std::move(font), settings, std::move(actions)},
        _samples{samples} {}
};

void summary(std::string_view name, std::vector<double> values) {
  test::require(!values.empty(), "native workload records samples");
  std::sort(values.begin(), values.end());
  std::cout << name << ',' << values.size() << ',' << values[values.size() / 2]
            << ','
            << values[std::min(values.size() - 1,
                               std::size_t(std::ceil(values.size() * .95) - 1))]
            << ',' << values.back() << '\n';
}
} // namespace

int main(int argc, char **argv) {
  if ((argc != 2 && argc != 3) || (std::string_view{argv[1]} != "software" &&
                                   std::string_view{argv[1]} != "gpu")) {
    std::cerr << "Usage: playground_ui_host_workload software|gpu [--burst]\n";
    return 2;
  }
  const bool burst = argc == 3 && std::string_view{argv[2]} == "--burst";
  if (argc == 3 && !burst)
    return 2;
  return test::run([&] {
    test::TemporaryDirectory user{"playground-ui-workload"};
    const bool gpu = std::string_view{argv[1]} == "gpu";
    std::ofstream{user.path() / "settings.toml"}
        << "schema_version = 5\n[graphics]\nrenderer = '"
        << (gpu ? "sdl-gpu" : "software") << "'\n";
    Samples samples;
    auto completed = samples.completed.get_future();
    ObservedSettings *view{};
    AppHost host{{.resizable = false},
                 {},
                 [&](auto &assets, auto font, auto settings, auto actions) {
                   auto result = std::make_unique<ObservedSettings>(
                       assets, font, settings, actions, samples);
                   view = result.get();
                   return result;
                 },
                 {.project = PLAYGROUND_SOURCE_DIR, .user = user.path()}};
    auto sink = host.completions();
    std::promise<Geometry> ready;
    auto geometry = ready.get_future();
    int stage{};
    const auto started = Clock::now();
    std::uint64_t frames{}, idleFrames{};
    std::exception_ptr failure;
    std::jthread producer{[&](std::stop_token stop) {
      try {
        while (geometry.wait_for(20ms) != std::future_status::ready &&
               !stop.stop_requested()) {
          if (Clock::now() - started > 15s)
            throw std::runtime_error("native workload did not become ready");
          sink.post([&] {
            if (host.windowRequestStatus().outcome ==
                platform::WindowTransitionOutcome::Pending)
              return;
            if (stage == 0) {
              auto policy = host.viewPolicy();
              policy.resizable = false;
              policy.initialSizing = platform::InitialWindowSizing::Preferred;
              policy.preferredWindowSize = math::Vec2i{408, 480};
              host.request(
                  {.type = AppCommandType::SetViewPolicy, .view = policy});
              stage = 1;
            } else if (stage == 1 &&
                       host.windowState().actualSize == math::Vec2i{408, 480}) {
              host.requestSettings();
              stage = 2;
            } else if (stage == 2 && host.settingsVisible() &&
                       Clock::now() - started > 1500ms) {
              auto *scroll = dynamic_cast<ui::ScrollView *>(
                  view->children().front().get());
              if (!scroll || scroll->viewportExtent().height <= 0)
                return;
              if (host.windowState().actualSize != math::Vec2i{408, 480} ||
                  scroll->contentExtent().height <=
                      scroll->viewportExtent().height) {
                stage = 3;
                ready.set_exception(std::make_exception_ptr(std::runtime_error(
                    "Workload requires a floating 408x480 window with "
                    "overflowing Settings; window manager changed geometry")));
                return;
              }
              const auto b = scroll->worldTransform().mapBounds(
                  {{}, scroll->bounds().size});
              const auto viewport = scroll->viewportExtent().height;
              const auto thumb = std::min(
                  viewport, std::max(*scroll->effectiveProps().minimumThumb,
                                     viewport * viewport /
                                         scroll->contentExtent().height));
              samples.geometry = {b.right() - 4, b.y() + 4,
                                  viewport - thumb - 4};
              frames = host.renderRuntimeState().submitted;
              ready.set_value(samples.geometry);
              stage = 3;
            }
          });
        }
        if (stop.stop_requested())
          return;
        const auto g = geometry.get();
        const auto mouse = [&](Uint32 type, float y) {
          SDL_Event e{};
          e.type = type;
          if (type == SDL_EVENT_MOUSE_MOTION) {
            e.motion.x = g.x;
            e.motion.y = y;
            e.motion.state = SDL_BUTTON_LMASK;
          } else {
            e.button.x = g.x;
            e.button.y = y;
            e.button.button = SDL_BUTTON_LEFT;
          }
          test::require(SDL_PushEvent(&e), "enqueue native workload event");
        };
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, g.y);
        for (int i = 0; i < moves; ++i) {
          samples.sent[i].store(ticks());
          mouse(SDL_EVENT_MOUSE_MOTION, g.y + g.travel * (i + 1) / moves);
          if (!burst)
            std::this_thread::sleep_for(8ms);
        }
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, g.y + g.travel);
        const auto deadline = Clock::now() + 15s;
        while (completed.wait_for(20ms) != std::future_status::ready &&
               !stop.stop_requested())
          if (Clock::now() >= deadline)
            throw std::runtime_error("final queued input was not painted");
        if (stop.stop_requested())
          return;
        sink.post([&] {
          std::cout << "backend," << int(host.rendererState().selected.backend)
                    << "\nwindow," << host.windowState().actualSize.x << ','
                    << host.windowState().actualSize.y << "\nframes,"
                    << host.renderRuntimeState().submitted - frames
                    << "\nlast_move_painted," << samples.painted << '\n';
          std::vector<double> poll;
          for (const auto &sample : host.renderTelemetry().cpu)
            poll.push_back(
                sample.milliseconds[std::size_t(rendering::CPUPhase::Poll)]);
          summary("poll_ms", poll);
          summary("enqueue_to_dispatch_ms", samples.dispatch);
          summary("enqueue_to_paint_ms", samples.paint);
          host.requestSettings(false);
        });
        std::this_thread::sleep_for(1s);
        sink.post([&] { idleFrames = host.renderRuntimeState().submitted; });
        std::this_thread::sleep_for(1s);
        sink.post([&] {
          std::cout << "menu_idle_frames,"
                    << host.renderRuntimeState().submitted - idleFrames << '\n';
          host.request({.type = AppCommandType::Quit});
        });
        for (int i = 0; i < 150 && !stop.stop_requested(); ++i)
          std::this_thread::sleep_for(20ms);
        if (!stop.stop_requested())
          throw std::runtime_error("native workload quit deadline expired");
      } catch (...) {
        failure = std::current_exception();
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
      }
    }};
    const auto result = host.run();
    producer.request_stop();
    producer.join();
    if (failure)
      std::rethrow_exception(failure);
    test::require(result == 0 && samples.painted == moves - 1 &&
                      host.lastCommandError().empty(),
                  "final input is painted and host completes without error");
    test::require((host.rendererState().selected.backend ==
                   rendering::RendererKind::SDLGPU) == gpu,
                  "requested renderer exercised");
  });
}
