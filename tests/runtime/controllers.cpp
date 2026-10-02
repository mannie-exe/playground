#include <cmath>
#include <limits>
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

    auto shot = [](float x, float yaw = 0) {
      return scene::FreeCameraController{{.position = {x, 0, 0}, .yaw = yaw}}
          .camera();
    };
    auto close = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
    auto a = shot(0), b = shot(10, std::numbers::pi_v<float> / 2);
    b.verticalFov = 0.8f;
    b.nearPlane = 0.2f;
    b.farPlane = 500;
    scene::CameraPath path(4, {{0, a}, {2, b}});
    const auto middle = path.sample(1);
    require(
        close(middle.eye.x, 5) &&
            close(middle.verticalFov, (a.verticalFov + b.verticalFov) / 2) &&
            close(middle.nearPlane, 0.15f) && close(middle.farPlane, 750),
        "path interpolates position and lens fields");
    const auto forward = math::normalized(middle.target - middle.eye);
    require(close(forward.x, std::sqrt(0.5f)) &&
                close(forward.z, std::sqrt(0.5f)),
            "camera orientation follows quaternion shortest arc");
    require(path.sample(0).eye == a.eye && path.sample(2).eye == b.eye &&
                path.sample(4).eye == a.eye &&
                path.sample(4000001).eye == middle.eye,
            "path keys and large elapsed time wrap deterministically");
    double elapsed = 0;
    for (int i = 0; i < 100; ++i)
      elapsed += 0.01;
    require(close(path.sample(elapsed).eye.x, middle.eye.x),
            "path sampling does not depend on time subdivision");
    const float epsilon = 0.001f;
    require(std::abs(path.sample(4 - epsilon).eye.x / epsilon) < 0.01f &&
                std::abs(path.sample(epsilon).eye.x / epsilon) < 0.01f,
            "loop seam position approaches zero velocity on both sides");
    const auto seamBefore = path.sample(4 - epsilon);
    const auto seamAfter = path.sample(epsilon);
    require(std::abs(math::normalized(seamBefore.target - seamBefore.eye).x /
                     epsilon) < 0.01f &&
                std::abs(math::normalized(seamAfter.target - seamAfter.eye).x /
                         epsilon) < 0.01f,
            "loop seam orientation approaches zero angular velocity");
    test::rejects([&] { path.sample(-1); }, "negative path elapsed rejected");
    test::rejects([&] { path.sample(INFINITY); },
                  "infinite path elapsed rejected");
    test::rejects([&] { scene::CameraPath invalid(0, {{0, a}, {1, b}}); },
                  "zero loop rejected");
    test::rejects([&] { scene::CameraPath invalid(2, {{0, a}, {0, b}}); },
                  "duplicate keys rejected");
    test::rejects([&] { scene::CameraPath invalid(2, {{1, a}, {1.5, b}}); },
                  "missing zero key rejected");
    test::rejects([&] { scene::CameraPath invalid(2, {{0, a}, {2, b}}); },
                  "duration endpoint key rejected");
    test::rejects([&] { scene::CameraPath invalid(2, {{0, a}, {NAN, b}}); },
                  "nonfinite key rejected");

    scene::CameraDirector director(a, 2);
    const auto first = director.add({b, 1});
    const auto tied = director.add({shot(20), 1});
    require(director.active() == first, "priority tie retains active source");
    director.advance(1);
    require(close(director.camera().eye.x, 5), "transition halfway pose");
    director.update(tied, {shot(20), 2});
    require(close(director.camera().eye.x, 5),
            "interruption preserves displayed pose");
    director.advance(1);
    require(close(director.camera().eye.x, 12.5f),
            "interruption blends from displayed pose");
    director.remove(tied);
    require(director.active() == first && close(director.camera().eye.x, 12.5f),
            "removing winner transitions to eligible source");
    director.advance(2);
    require(close(director.camera().eye.x, 10) && !director.transitioning(),
            "transition settles exactly on target");
    test::rejects([&] { director.remove(tied); }, "removed camera ID rejected");
    const auto replacement = director.add({shot(30), 0});
    require(replacement != tied, "removed IDs never reused");
    scene::CameraDirector foreign;
    const auto foreignId = foreign.add({a});
    test::rejects([&] { director.update(foreignId, {a}); },
                  "foreign director ID rejected");
    auto invalid = b;
    invalid.eye.x = INFINITY;
    test::rejects([&] { director.update(first, {invalid, 100}); },
                  "invalid source rejected");
    test::rejects([&] { director.add({invalid, 100}); },
                  "invalid registration rejected");
    test::rejects([&] { director.setFallback(invalid); },
                  "invalid fallback rejected");
    test::rejects([&] { director.advance(NAN); }, "nonfinite advance rejected");
    test::rejects([&] { director.advance(-1); }, "negative advance rejected");
    require(director.active() == first && director.camera().eye == b.eye &&
                !director.transitioning(),
            "invalid director operations leave state unchanged");
    director.remove(replacement);
    director.remove(first);
    require(!director.active() && director.camera().eye == b.eye &&
                !director.transitioning(),
            "empty director holds last displayed camera");
    director.setFallback(a);
    director.advance(2);
    require(director.camera().eye == a.eye,
            "empty director transitions to explicit fallback");

    scene::CameraDirector stable(a, 0);
    const auto lowId = stable.add({b, 1});
    const auto otherId = stable.add({shot(20), 1});
    const auto highId = stable.add({shot(30), 2});
    stable.remove(highId);
    require(stable.active() == lowId,
            "ties without active winner use stable registration ID");
    stable.update(lowId, {b, 1, false});
    require(stable.active() == otherId, "disabled source is ineligible");
    stable.update(lowId, {b, 1});
    require(stable.active() == otherId,
            "reenabling earlier tied ID retains active camera");

    auto ortho = b;
    ortho.orthographicHeight = 4;
    director.add({ortho});
    require(!director.transitioning() &&
                director.camera().orthographicHeight == 4,
            "projection mode switch cuts immediately");
    scene::CameraPath cuts(4, {{0, a}, {2, ortho}});
    require(!cuts.sample(1).orthographicHeight &&
                cuts.sample(2).orthographicHeight == 4 &&
                cuts.sample(3).orthographicHeight == 4 &&
                !cuts.sample(4).orthographicHeight,
            "mixed projection path holds until next key cut");
    auto orthoEnd = ortho;
    orthoEnd.orthographicHeight = 8;
    scene::CameraPath orthographic(4, {{0, ortho}, {2, orthoEnd}});
    require(orthographic.sample(1).orthographicHeight == 6,
            "orthographic height interpolates");
    orthoEnd.verticalFov = NAN;
    test::rejects(
        [&] { director.update(director.active().value(), {orthoEnd}); },
        "inactive perspective lens field also validated");
    auto opposite = a;
    opposite.target = {0, 0, -1};
    scene::CameraPath turn(2, {{0, a}, {1, opposite}});
    turn.sample(0.5).view(1);
    auto pole = a;
    pole.target = {0, 1, 0};
    pole.up = {0, 0, -1};
    scene::CameraPath vertical(2, {{0, a}, {1, pole}});
    vertical.sample(0.5).view(1);
    const auto left = shot(0, 170 * std::numbers::pi_v<float> / 180);
    const auto right = shot(0, -170 * std::numbers::pi_v<float> / 180);
    scene::CameraPath shortArc(2, {{0, left}, {1, right}});
    const auto arcMiddle = shortArc.sample(0.5);
    require(math::normalized(arcMiddle.target - arcMiddle.eye).z < -0.999f,
            "quaternion transition takes shortest arc across yaw wrap");
    auto rolled = a;
    rolled.up = {1, 0, 0};
    scene::CameraPath roll(2, {{0, a}, {1, rolled}});
    require(close(roll.sample(0.5).up.x, std::sqrt(0.5f)) &&
                close(roll.sample(0.5).up.y, std::sqrt(0.5f)),
            "quaternion interpolation preserves authored roll");
    scene::CameraDirector hold(a, 2);
    auto heldId = hold.add({b});
    hold.advance(1);
    hold.remove(heldId);
    hold.advance(10);
    require(close(hold.camera().eye.x, 5) && !hold.transitioning(),
            "removal without fallback holds interrupted displayed pose");
    hold.setFallback(a);
    heldId = hold.add({b});
    hold.advance(2);
    hold.remove(heldId);
    hold.advance(1);
    require(close(hold.camera().eye.x, 5) && !hold.active(),
            "removal uses explicit fallback when no sources remain");
    test::rejects([&] { hold.update(heldId, {a}); },
                  "stale camera update rejected");
    auto singular = a;
    singular.up = {0, 0, 1};
    test::rejects(
        [&] { scene::CameraPath invalid(2, {{0, a}, {1, singular}}); },
        "singular camera basis rejected during path construction");
    test::rejects([&] { scene::CameraDirector invalid(a, -1); },
                  "negative transition duration rejected");

    scene::CameraDirector one(a, 2), many(a, 2);
    one.add({b});
    many.add({b});
    one.advance(1);
    for (int i = 0; i < 10; ++i)
      many.advance(0.1);
    require(close(one.camera().eye.x, many.camera().eye.x),
            "director transition is independent of time subdivision");
    many.advance(std::numeric_limits<double>::max());
    require(!many.transitioning() && many.camera().eye == b.eye,
            "large finite transition advance settles without overflow");

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
