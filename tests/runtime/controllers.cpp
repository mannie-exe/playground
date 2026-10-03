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
constexpr world::SpaceId space{{1}, 1};
constexpr world::RenderOrigin origin{{space, {}}, 1};

math::Vec3f forward(const scene::WorldCamera &c) {
  const auto v = world::rotate(c.pose.orientation, {0, 0, 1});
  return {float(v.x), float(v.y), float(v.z)};
}

math::Vec3f up(const scene::WorldCamera &c) {
  const auto v = world::rotate(c.pose.orientation, {0, 1, 0});
  return {float(v.x), float(v.y), float(v.z)};
}

int main() {
  return test::run([] {
    scene::MovementController movement{{.unitsPerSecond = 2}};
    auto position = movement.advance({space, {}}, {{1, 0, 1}}, 1);
    require(std::abs(std::hypot(position.meters.x, position.meters.z) - 2) <
                1e-5,
            "diagonal speed bounded");
    require(movement.advance({space, {1, 2, 3}}, {}, 1) ==
                world::WorldPosition{space, {1, 2, 3}},
            "zero intent stationary");
    scene::OrbitController orbit{{.target = {space, {}}, .epoch = 1}};
    require(orbit.camera().pose.position.meters == world::Vec3d{0, 0, -3},
            "default matches LH camera");
    orbit.update(
        {.radians = {std::numbers::pi_v<float> / 2, 100}, .zoom = -100});
    require(orbit.props().distance == 0.1 &&
                orbit.props().pitch == orbit.props().maximumPitch,
            "orbit clamps distance and poles");
    orbit.camera().localCamera(origin, {}).view(1);
    bool rejected{};
    try {
      orbit.setProps({.target = {space, {}}, .epoch = 1, .distance = -1});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    require(rejected && orbit.props().distance == 0.1,
            "invalid props leave camera unchanged");
    {
      const scene::OrbitProps initial{
          .target = {space, {1e6, 0, 0}}, .epoch = 1, .distance = 10};
      scene::OrbitController whole{initial}, split{initial};
      whole.pan({40, 20}, 400);
      split.pan({20, 10}, 400);
      split.pan({20, 10}, 400);
      require(
          world::length(world::relativeTo(whole.props().target,
                                          split.props().target)) < 1e-8 &&
              whole.props().target.meters.x < 1e6 &&
              whole.props().target.meters.y > 0,
          "pixel pan preserves precision and subdivision at distant origins");
      const auto before = whole.camera();
      test::rejects([&] { whole.pan({1, 2}, 0); },
                    "empty pan viewport rejects");
      require(whole.camera() == before, "invalid pan leaves camera unchanged");
      whole.update({.zoomLog = std::log(.5)});
      scene::OrbitController bounded{whole.props()};
      bounded.update({.zoomLog = 1e6});
      require(bounded.props().distance == bounded.props().maximumDistance,
              "large positive log zoom saturates at maximum distance");
      bounded.update({.zoomLog = -1e6});
      require(bounded.props().distance == bounded.props().minimumDistance,
              "large negative log zoom saturates at minimum distance");
      require(std::abs(whole.props().distance - 5) < 1e-10,
              "perspective wheel zoom scales distance proportionally");
      auto ortho = initial;
      ortho.lens.orthographicHeight = 8;
      scene::OrbitController orthographic{ortho};
      orthographic.update({.zoomLog = std::log(.5)});
      require(std::abs(*orthographic.camera().lens.orthographicHeight - 4) <
                      1e-5 &&
                  orthographic.props().distance == 10,
              "orthographic zoom scales view height without dollying");
      orthographic.pan({100, 0}, 400);
      require(std::abs(orthographic.props().target.meters.x - (1e6 - 1)) < 1e-8,
              "orthographic pan derives scale from view height");
    }

    scene::FreeCameraController fly{
        {.position = {space, {0, 0, -3}}, .epoch = 1, .unitsPerSecond = 2}};
    fly.advance({.movement = {1, 0, 1}}, 1);
    require(std::abs(std::hypot(fly.props().position.meters.x,
                                fly.props().position.meters.z + 3) -
                     2) < 1e-5,
            "free camera bounds diagonal movement speed");
    scene::FreeCameraController whole{{.position = {space, {}}, .epoch = 1}},
        split{{.position = {space, {}}, .epoch = 1}};
    whole.advance({.movement = {0, 0, 1}}, 1);
    for (int i = 0; i < 10; ++i)
      split.advance({.movement = {0, 0, 1}}, .1);
    require(std::abs(whole.props().position.meters.z -
                     split.props().position.meters.z) < 1e-5,
            "held free movement is time based");
    const auto before = fly.props().position;
    test::rejects([&] { fly.advance({.radians = {INFINITY, 0}}, 1); },
                  "non-finite look rejected before mutation");
    require(fly.props().position == before, "invalid camera intent is atomic");
    fly.advance({.radians = {std::numbers::pi_v<float> / 2, 100}}, 0);
    fly.camera().localCamera(origin, {}).view(1);
    require(fly.props().pitch == fly.props().maximumPitch &&
                fly.props().position == before,
            "look displacement clamps poles independently of elapsed time");

    auto shot = [](float x, float yaw = 0) {
      return scene::FreeCameraController{
          {.position = {space, {x, 0, 0}}, .epoch = 1, .yaw = yaw}}
          .camera();
    };
    auto close = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
    auto a = shot(0), b = shot(10, std::numbers::pi_v<float> / 2);
    b.lens.verticalFov = 0.8f;
    b.lens.nearPlane = 0.2f;
    b.lens.farPlane = 500;
    scene::CameraPath path(4, {{0, a}, {2, b}});
    const auto middle = path.sample(1);
    require(close(middle.pose.position.meters.x, 5) &&
                close(middle.lens.verticalFov,
                      (a.lens.verticalFov + b.lens.verticalFov) / 2) &&
                close(middle.lens.nearPlane, 0.15f) &&
                close(middle.lens.farPlane, 750),
            "path interpolates position and lens fields");
    const auto direction = forward(middle);
    require(close(direction.x, std::sqrt(0.5f)) &&
                close(direction.z, std::sqrt(0.5f)),
            "camera orientation follows quaternion shortest arc");
    require(path.sample(0).pose.position.meters == a.pose.position.meters &&
                path.sample(2).pose.position.meters == b.pose.position.meters &&
                path.sample(4).pose.position.meters == a.pose.position.meters &&
                path.sample(4000001).pose.position.meters ==
                    middle.pose.position.meters,
            "path keys and large elapsed time wrap deterministically");
    double elapsed = 0;
    for (int i = 0; i < 100; ++i)
      elapsed += 0.01;
    require(close(path.sample(elapsed).pose.position.meters.x,
                  middle.pose.position.meters.x),
            "path sampling does not depend on time subdivision");
    const float epsilon = 0.001f;
    require(std::abs(path.sample(4 - epsilon).pose.position.meters.x /
                     epsilon) < 0.01f &&
                std::abs(path.sample(epsilon).pose.position.meters.x /
                         epsilon) < 0.01f,
            "loop seam position approaches zero velocity on both sides");
    const auto seamBefore = path.sample(4 - epsilon);
    const auto seamAfter = path.sample(epsilon);
    require(std::abs(forward(seamBefore).x / epsilon) < 0.01f &&
                std::abs(forward(seamAfter).x / epsilon) < 0.01f,
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
    require(close(director.camera().pose.position.meters.x, 5),
            "transition halfway pose");
    director.update(tied, {shot(20), 2});
    require(close(director.camera().pose.position.meters.x, 5),
            "interruption preserves displayed pose");
    director.advance(1);
    require(close(director.camera().pose.position.meters.x, 12.5f),
            "interruption blends from displayed pose");
    director.remove(tied);
    require(director.active() == first &&
                close(director.camera().pose.position.meters.x, 12.5f),
            "removing winner transitions to eligible source");
    director.advance(2);
    require(close(director.camera().pose.position.meters.x, 10) &&
                !director.transitioning(),
            "transition settles exactly on target");
    test::rejects([&] { director.remove(tied); }, "removed camera ID rejected");
    const auto replacement = director.add({shot(30), 0});
    require(replacement != tied, "removed IDs never reused");
    scene::CameraDirector foreign(a);
    const auto foreignId = foreign.add({a});
    test::rejects([&] { director.update(foreignId, {a}); },
                  "foreign director ID rejected");
    auto invalid = b;
    invalid.pose.position.meters.x = INFINITY;
    test::rejects([&] { director.update(first, {invalid, 100}); },
                  "invalid source rejected");
    test::rejects([&] { director.add({invalid, 100}); },
                  "invalid registration rejected");
    test::rejects([&] { director.setFallback(invalid); },
                  "invalid fallback rejected");
    test::rejects([&] { director.advance(NAN); }, "nonfinite advance rejected");
    test::rejects([&] { director.advance(-1); }, "negative advance rejected");
    require(director.active() == first &&
                director.camera().pose.position.meters ==
                    b.pose.position.meters &&
                !director.transitioning(),
            "invalid director operations leave state unchanged");
    director.remove(replacement);
    director.remove(first);
    require(!director.active() &&
                director.camera().pose.position.meters ==
                    b.pose.position.meters &&
                !director.transitioning(),
            "empty director holds last displayed camera");
    director.setFallback(a);
    director.advance(2);
    require(director.camera().pose.position.meters == a.pose.position.meters,
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
    ortho.lens.orthographicHeight = 4;
    director.add({ortho});
    require(!director.transitioning() &&
                director.camera().lens.orthographicHeight == 4,
            "projection mode switch cuts immediately");
    scene::CameraPath cuts(4, {{0, a}, {2, ortho}});
    require(!cuts.sample(1).lens.orthographicHeight &&
                cuts.sample(2).lens.orthographicHeight == 4 &&
                cuts.sample(3).lens.orthographicHeight == 4 &&
                !cuts.sample(4).lens.orthographicHeight,
            "mixed projection path holds until next key cut");
    auto orthoEnd = ortho;
    orthoEnd.lens.orthographicHeight = 8;
    scene::CameraPath orthographic(4, {{0, ortho}, {2, orthoEnd}});
    require(orthographic.sample(1).lens.orthographicHeight == 6,
            "orthographic height interpolates");
    orthoEnd.lens.verticalFov = NAN;
    test::rejects(
        [&] { director.update(director.active().value(), {orthoEnd}); },
        "inactive perspective lens field also validated");
    auto opposite = a;
    opposite.pose.orientation = math::lookRotation({0, 0, -1});
    scene::CameraPath turn(2, {{0, a}, {1, opposite}});
    turn.sample(0.5).localCamera(origin, {}).view(1);
    auto pole = a;
    pole.pose.orientation = math::lookRotation({0, 1, 0}, {0, 0, -1});
    scene::CameraPath vertical(2, {{0, a}, {1, pole}});
    vertical.sample(0.5).localCamera(origin, {}).view(1);
    const auto left = shot(0, 170 * std::numbers::pi_v<float> / 180);
    const auto right = shot(0, -170 * std::numbers::pi_v<float> / 180);
    scene::CameraPath shortArc(2, {{0, left}, {1, right}});
    const auto arcMiddle = shortArc.sample(0.5);
    require(forward(arcMiddle).z < -0.999f,
            "quaternion transition takes shortest arc across yaw wrap");
    auto rolled = a;
    rolled.pose.orientation = math::lookRotation({0, 0, 1}, {1, 0, 0});
    scene::CameraPath roll(2, {{0, a}, {1, rolled}});
    require(close(up(roll.sample(0.5)).x, std::sqrt(0.5f)) &&
                close(up(roll.sample(0.5)).y, std::sqrt(0.5f)),
            "quaternion interpolation preserves authored roll");
    scene::CameraDirector hold(a, 2);
    auto heldId = hold.add({b});
    hold.advance(1);
    hold.remove(heldId);
    hold.advance(10);
    require(close(hold.camera().pose.position.meters.x, 5) &&
                !hold.transitioning(),
            "removal without fallback holds interrupted displayed pose");
    hold.setFallback(a);
    heldId = hold.add({b});
    hold.advance(2);
    hold.remove(heldId);
    hold.advance(1);
    require(close(hold.camera().pose.position.meters.x, 5) && !hold.active(),
            "removal uses explicit fallback when no sources remain");
    test::rejects([&] { hold.update(heldId, {a}); },
                  "stale camera update rejected");
    auto singular = a;
    singular.pose.orientation = {0, 0, 0, 0};
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
    require(close(one.camera().pose.position.meters.x,
                  many.camera().pose.position.meters.x),
            "director transition is independent of time subdivision");
    many.advance(std::numeric_limits<double>::max());
    require(!many.transitioning() &&
                many.camera().pose.position.meters == b.pose.position.meters,
            "large finite transition advance settles without overflow");

    world::EntitySample actual{.entity = {{{1}, 1}, 1},
                               .pose = {{space, {}}, {}},
                               .velocity = {space},
                               .discontinuity = 1};
    scene::PoseHistory poses(actual);
    runtime::SimulationClock clock{{.stepSeconds = 0.01}};
    clock.beginFrame(0.025);
    while (auto step = clock.nextStep()) {
      actual.pose.position =
          movement.advance(actual.pose.position, {{1, 0, 0}}, step->seconds);
      ++actual.tick;
      poses.publish(actual);
    }
    require(
        std::abs(poses.sample(clock.state().alpha).pose.position.meters.x -
                 .03) < 1e-6,
        "fixed poses interpolate into presentation without mutating authority");
    actual.pose.position.meters.x = 10;
    ++actual.discontinuity;
    ++actual.tick;
    poses.publish(actual);
    require(poses.sample(0).pose.position.meters.x == 10 &&
                poses.sample(1).pose.position.meters.x == 10,
            "teleport does not interpolate across discontinuity");
    actual.pose.orientation = {0, 0, 0, -1};
    ++actual.tick;
    poses.publish(actual);
    require(std::abs(poses.sample(.5).pose.orientation.w) == 1,
            "opposite quaternion signs do not collapse");
    test::rejects([&] { poses.publish(actual); },
                  "duplicate simulation sample rejected");
    ++actual.entity.epoch;
    actual.tick = 0;
    actual.pose.position.meters.x = 20;
    poses.publish(actual);
    require(poses.sample(0).pose.position.meters.x == 20,
            "new incarnation resets interpolation");

    scene::CameraDirector moving(a, 1);
    const auto movingId = moving.add({b});
    for (int i = 0; i < 10; ++i) {
      moving.update(movingId, {shot(10 + i)});
      moving.advance(.1);
    }
    moving.advance(.01);
    require(!moving.transitioning() &&
                close(moving.camera().pose.position.meters.x, 19),
            "continuous destination updates do not restart transition");
    auto distant = a;
    distant.pose.position.meters.x = 1e8;
    auto precise = distant;
    precise.pose.position.meters.x += .01;
    scene::CameraPath precisePath(2, {{0, distant}, {1, precise}});
    require(std::abs(precisePath.sample(.5).pose.position.meters.x - 1e8 -
                     .005) < 1e-7,
            "world path keeps centimeter precision far from origin");
    world::SpatialLimits largeLimits{.maximumCoordinate = 3e9};
    auto enormous = a;
    enormous.pose.position.meters.x = 2e9;
    scene::CameraDirector largeDirector(enormous, 0, largeLimits);
    scene::CameraPath largePath(2, {{0, enormous}, {1, enormous}}, largeLimits);
    require(largePath.sample(.5).pose.position.meters.x == 2e9 &&
                largeDirector.camera().pose.position.meters.x == 2e9,
            "director and paths respect declared space limits instead of "
            "global defaults");
    auto otherEpoch = a;
    ++otherEpoch.epoch;
    moving.update(movingId, {otherEpoch});
    require(!moving.transitioning() &&
                moving.camera().epoch == otherEpoch.epoch,
            "epoch changes cut");
    test::rejects(
        [&] { scene::CameraPath invalid(2, {{0, a}, {1, otherEpoch}}); },
        "mixed epoch path rejected");
    scene::LookController lookWhole, lookSplit;
    lookWhole.advance({{.2f, .1f}, {.5f, 0}}, 1);
    lookSplit.advance({{.2f, .1f}, {}}, 0);
    for (int i = 0; i < 10; ++i)
      lookSplit.advance({{}, {.5f, 0}}, .1);
    require(close(lookWhole.state().yaw, lookSplit.state().yaw),
            "look rates integrate while displacement applies once");

    auto target = scene::cameraTarget(poses.current());
    scene::FollowCameraRig rig;
    rig.reset(target, {});
    require(rig.state().effective == scene::CameraPerspective::ThirdPerson &&
                !rig.state().transitioning,
            "follow reset starts settled third person");
    rig.advance(target, {}, {-3, 0}, .016);
    require(
        rig.state().effective == scene::CameraPerspective::FirstPerson &&
            rig.state().transitioning,
        "zoom threshold enters first person with independent transition state");
    rig.advance(target, {}, {.2, 0}, .016);
    require(rig.state().effective == scene::CameraPerspective::FirstPerson,
            "perspective hysteresis retains mode");
    rig.advance(target, {}, {.2, 0}, .016);
    require(rig.state().effective == scene::CameraPerspective::ThirdPerson,
            "zoom exits first person above upper threshold");
    const auto beforeInvalid = rig.camera();
    test::rejects([&] { rig.advance(target, {}, {NAN, 0}, 1); },
                  "invalid zoom rejected");
    require(rig.camera() == beforeInvalid,
            "failed evaluation leaves rig unchanged");
    auto reduced = rig.props();
    reduced.reducedMotion = true;
    reduced.perspective = scene::PerspectivePolicy::FirstPerson;
    rig.setProps(reduced);
    rig.advance(target, {.2, .3}, {}, 0);
    require(
        !rig.state().transitioning &&
            rig.camera().pose.position == target.eyeAnchor &&
            rig.state().obstruction ==
                scene::CameraObstructionStatus::Unavailable,
        "reduced motion snaps placement and obstruction remains unavailable");
    scene::FollowCameraRig adopted;
    adopted.reset(target, {.4, .1});
    const auto view = adopted.camera();
    require(adopted.adoptView(view, target).exact,
            "reachable outgoing view adopts exactly");
    auto unreachable = view;
    unreachable.pose.position.meters.y += 10;
    require(!adopted.adoptView(unreachable, target).exact,
            "unreachable adoption reports constraint");
    target.subject.discontinuity++;
    target.followAnchor.meters.x += 100;
    target.eyeAnchor.meters.x += 100;
    adopted.advance(target, {}, {}, .001);
    require(!adopted.state().transitioning,
            "discontinuity resets follow damping");
    const auto savedLook = scene::LookState{.6, .2};
    adopted.returnView(scene::CameraReturnPolicy::RestoreSavedView, view,
                       target, savedLook);
    require(world::length(
                world::rotate(adopted.camera().pose.orientation, {0, 0, 1}) -
                world::rotate(scene::LookController{savedLook}.orientation(),
                              {0, 0, 1})) < 1e-6,
            "return publishes saved rig target for director handoff without a "
            "second blend");
    adopted.clearTarget();
    test::rejects<std::logic_error>([&] { adopted.camera(); },
                                    "removed target exposes no stale camera");
  });
}
