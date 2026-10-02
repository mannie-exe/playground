#include <cmath>
#include <numbers>
#include <stdexcept>

#include <input/InputMap.hpp>
#include <runtime/SimulationClock.hpp>
#include <scene/Controllers.hpp>
#include <support/Test.hpp>

using playground::test::require;
using namespace playground;

int main() {
  return test::run([] {
    scene::MovementController movement{{.unitsPerSecond = 2}};
    auto position = movement.advance({}, {{1, 0, 1}}, 1);
    require(std::abs(std::hypot(position.x, position.z) - 2) < 1e-5,
            "diagonal speed bounded");
    require(movement.advance({1, 2, 3}, {}, 1) == math::Vec3f{1, 2, 3},
            "zero intent stationary");
    scene::OrbitController orbit;
    require(orbit.camera().eye == math::Vec3f{0, 0, -3},
            "default matches LH camera");
    orbit.update(
        {.radians = {std::numbers::pi_v<float> / 2, 100}, .zoom = -100});
    require(orbit.props().distance == 0.1f &&
                orbit.props().pitch == orbit.props().maximumPitch,
            "orbit clamps distance and poles");
    orbit.camera().view(1);
    bool rejected{};
    try {
      orbit.setProps({.distance = -1});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected && orbit.props().distance == 0.1f,
            "invalid props leave camera unchanged");

    scene::FreeCameraController fly{{.unitsPerSecond = 2}};
    fly.advance({.movement = {1, 0, 1}}, 1);
    require(std::abs(
                std::hypot(fly.props().position.x, fly.props().position.z + 3) -
                2) < 1e-5,
            "free camera bounds diagonal movement speed");
    scene::FreeCameraController whole, split;
    whole.advance({.movement = {0, 0, 1}}, 1);
    for (int i = 0; i < 10; ++i)
      split.advance({.movement = {0, 0, 1}}, .1);
    require(std::abs(whole.props().position.z - split.props().position.z) <
                1e-5,
            "held free movement is time based");
    const auto before = fly.props().position;
    test::rejects([&] { fly.advance({.radians = {INFINITY, 0}}, 1); },
                  "non-finite look rejected before mutation");
    require(fly.props().position == before, "invalid camera intent is atomic");
    fly.advance({.radians = {std::numbers::pi_v<float> / 2, 100}}, 0);
    fly.camera().view(1);
    require(fly.props().pitch == fly.props().maximumPitch &&
                fly.props().position == before,
            "look displacement clamps poles independently of elapsed time");

    scene::PoseHistory poses;
    scene::Scene3D world;
    auto id = world.create();
    runtime::SimulationClock clock{{.stepSeconds = 0.01}};
    clock.beginFrame(0.025);
    while (auto step = clock.nextStep()) {
      auto pose = poses.current();
      pose.position =
          movement.advance(pose.position, {{1, 0, 0}}, step->seconds);
      poses.publish(pose);
    }
    world.applyPatch(id, {.transform = poses.sample(clock.state().alpha)});
    require(std::abs(world.props(id).transform.position.x - 0.03f) < 1e-6,
            "moving object fixed steps interpolate into authored scene state");
    poses.teleport({.position = {10, 0, 0}});
    require(poses.sample(0).position.x == 10 &&
                poses.sample(1).position.x == 10,
            "teleport does not interpolate across discontinuity");
    poses.publish({.position = {10, 0, 0}, .orientation = {0, 0, 0, -1}});
    require(std::abs(poses.sample(0.5).orientation.w) == 1,
            "opposite quaternion signs do not collapse");
  });
}
