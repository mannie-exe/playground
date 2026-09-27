#include <array>
#include <cmath>
#include <cstring>
#include <string>

#include <scene/ModelImport.hpp>
#include <support/Test.hpp>

using namespace playground;

int main() {
  return test::run([] {
    const std::string json =
        R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":64,"uri":"tracks.bin"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24},{"buffer":0,"byteOffset":32,"byteLength":32}],"accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":2,"type":"VEC4"}],"nodes":[{"name":"animated"}],"scenes":[{"nodes":[0]}],"animations":[{"name":"move","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"},{"input":0,"output":2,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}},{"sampler":1,"target":{"node":0,"path":"rotation"}}]}]})";
    const std::array<float, 16> values{
        0, 2, 0, 0, 0, 2, 0, 4, 0, 0, 0, 1, 0, .70710678f, 0, .70710678f};
    scene::ModelImportServices services{
        .readResource = [&](std::string_view uri) {
          test::require(uri == "tracks.bin", "explicit animation dependency");
          const auto bytes = std::as_bytes(std::span{values});
          return std::vector<std::byte>{bytes.begin(), bytes.end()};
        }};
    auto model =
        scene::importGLTF(std::as_bytes(std::span{json.data(), json.size()}),
                          services, {.unitsPerMeter = 2});
    test::require(model->clips().size() == 1 && model->clips()[0].duration == 2,
                  "rigid clip imported");
    scene::Scene3D world;
    auto instance = model->instantiate(world);
    auto other = model->instantiate(world);
    scene::Playback playback;
    playback.seek(1);
    model->applyAnimation(world, instance, 0, playback);
    const auto &pose = world.props(instance.nodes[0]).transform;
    test::require(pose.position == math::Vec3f{2, 0, -4},
                  "translation samples and handedness/unit conversion");
    test::require(std::abs(pose.orientation.y + std::sin(.3926990817f)) < 1e-5f,
                  "rotation uses shortest-arc slerp");
    test::require(world.props(other.nodes[0]).transform.position ==
                      math::Vec3f{},
                  "instances do not share playback state");
    auto unrelated = std::make_shared<const scene::ModelAsset>(
        model->nodes(), model->warnings(), model->clips());
    test::rejects(
        [&] { unrelated->applyAnimation(world, instance, 0, playback); },
        "matching node count is not model identity");
    playback.seek(2);
    test::require(playback.sampleTime(2) == 0, "loop endpoint wraps");
    playback.seek(-.5);
    test::require(playback.sampleTime(2) == 1.5, "negative playback wraps");
    playback.setProps({scene::PlaybackMode::Clamp, 1});
    playback.seek(3);
    test::require(playback.sampleTime(2) == 2, "clamp endpoint preserved");
    playback.setPaused(true);
    playback.advance(10);
    test::require(playback.time() == 3, "pause does not advance");
    scene::TransformTrack step{0,
                               scene::TrackPath::Scale,
                               scene::TrackInterpolation::Step,
                               {0, 2},
                               {{1, 1, 1, 0}, {2, 2, 2, 0}}};
    step.validate(1);
    test::require(step.sample(1).x == 1 && step.sample(2).x == 2,
                  "STEP boundaries");
    scene::TransformTrack cubic{
        0,
        scene::TrackPath::Translation,
        scene::TrackInterpolation::CubicSpline,
        {0, 2},
        {{}, {0, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {2, 0, 0, 0}, {}}};
    cubic.validate(1);
    test::require(std::abs(cubic.sample(1).x - 1) < 1e-6f,
                  "Hermite tangents scaled by interval");
    step.times = {1, 1};
    test::rejects([&] { step.validate(1); }, "duplicate times rejected");
    world.remove(instance.nodes[0]);
    const auto revision = world.revision();
    test::rejects([&] { model->applyAnimation(world, instance, 0, playback); },
                  "stale animation target rejected");
    test::require(revision == world.revision(),
                  "stale instance does not partially update");
    scene::Playback flip;
    flip.seek(.15);
    auto uv = scene::flipbookFrame(
        {.grid = {2, 2}, .frames = 4, .framesPerSecond = 10}, flip);
    test::require(uv.offset == math::Vec2f{.5f, 0} &&
                      uv.scale == math::Vec2f{.5f, .5f},
                  "row-major flipbook selection");
    test::rejects([] { scene::FlipbookProps{.grid = {0, 1}}.validate(); },
                  "invalid atlas rejected");
    flip.seek(29. / 15.);
    uv = scene::flipbookFrame(
        {.grid = {6, 5},
         .frames = 30,
         .framesPerSecond = 15,
         .pixels = scene::FlipbookProps::PixelGrid{{1536, 1279}, {256, 256}}},
        flip);
    test::require(std::abs(uv.offset.y + uv.scale.y - 1) < 1e-6f,
                  "cropped atlas trailing frame stays inside source");
    const scene::CameraProps camera{.eye = {2, 1, -4}, .up = {1, 1, 0}};
    const auto quad = scene::billboard({}, camera).matrix();
    const auto observer = math::inverse(camera.view(1).view);
    for (const auto axis :
         {math::Vec3f{1, 0, 0}, math::Vec3f{0, 1, 0}, math::Vec3f{0, 0, 1}}) {
      const auto delta = math::transformDirection(quad, axis) -
                         math::transformDirection(observer, axis);
      test::require(math::dot(delta, delta) < 1e-10f,
                    "billboard matches camera basis including roll");
    }
  });
}
