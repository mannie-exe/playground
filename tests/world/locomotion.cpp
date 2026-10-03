#include <array>
#include <cmath>
#include <numbers>

#include <support/Test.hpp>
#include <world/Locomotion.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

namespace {
bool near(double a, double b, double tolerance = 1e-5) {
  return std::abs(a - b) <= tolerance;
}
} // namespace

int main() {
  return test::run([] {
    const SpaceId space{{117}, 1};
    LocomotionState state{
        {{space.world, 1}, 8}, {{space, {1e6, 0, 0}}, {}}, {space}, 1, 1};
    auto responsive = LocomotionProps::responsive();
    CharacterFacingController exploration{responsive};
    const LocomotionIntent rightward{.forward = 1,
                                     .viewHeading = std::numbers::pi / 2};
    auto request = exploration.advance(state, rightward, .1);
    auto moved = realizeMovement(state, request, .1);
    require(near(moved.actual.pose.position.meters.x, 1e6 + .3) &&
                near(moved.actual.pose.position.meters.z, 0) &&
                near(request.resolvedHeading, std::numbers::pi / 2),
            "responsive exploration moves and faces view-relative travel");
    require(state.pose.position.meters.x == 1e6,
            "evaluation never changes authoritative state");
    CharacterFacingController natural;
    auto gradual = natural.advance(state, rightward, .1);
    require(near(gradual.velocity.linear.x, 0) &&
                near(gradual.velocity.linear.z, 3) &&
                gradual.resolvedHeading > 0 &&
                gradual.resolvedHeading < std::numbers::pi / 2,
            "natural steering uses actual body basis while facing catches up");
    auto diagonal = exploration.advance(state, {.right = 1, .forward = 1}, .1);
    require(near(length(diagonal.velocity.linear), 3),
            "diagonal input does not increase speed");
    auto analog = exploration.advance(state, {.forward = .25}, .1);
    require(near(length(analog.velocity.linear), .75),
            "analog magnitude survives normalization");
    auto invalid = responsive;
    invalid.basis = MovementBasis::Body;
    test::rejects([&] { exploration.setProps(invalid); },
                  "self-chasing travel-facing body basis rejected");
    require(exploration.props().basis == MovementBasis::View,
            "invalid configuration preserves active controller");
    auto zero =
        realizeMovement(state, exploration.advance(state, rightward, 0), 0);
    require(zero.actual.pose == state.pose,
            "zero elapsed time never realizes motion");

    auto strafe = responsive;
    strafe.profile = LocomotionProfile::Strafe;
    CharacterFacingController strafing{strafe};
    auto side = strafing.advance(state, {.right = 1}, .1);
    require(side.velocity.linear.x > 0 && near(side.resolvedHeading, 0),
            "strafe preserves aim while moving laterally");
    auto tank = responsive;
    tank.profile = LocomotionProfile::Tank;
    CharacterFacingController driving{tank};
    auto turning = driving.advance(state, {.right = 1, .viewHeading = 2}, .1);
    require(turning.velocity.linear == Vec3d{} && turning.resolvedHeading > 0,
            "tank turns in place without camera-relative strafe");
    auto reverse =
        driving.advance(state, {.forward = -1, .viewHeading = 2}, .1);
    require(reverse.velocity.linear.z == -3 && near(reverse.resolvedHeading, 0),
            "tank reverse ignores orbit heading");

    auto limited = responsive;
    limited.response = TurnResponse::RateLimited;
    limited.turnRate = 1;
    CharacterFacingController bounded{limited};
    require(near(bounded.advance(state, rightward, .1).resolvedHeading, .1),
            "turn rate uses seconds");
    auto damped = responsive;
    damped.response = TurnResponse::Damped;
    CharacterFacingController one{damped}, split{damped};
    auto oneStep = one.advance(state, rightward, .2);
    auto half =
        realizeMovement(state, split.advance(state, rightward, .1), .1).actual;
    auto twoSteps = split.advance(half, rightward, .1);
    require(near(oneStep.resolvedHeading, twoSteps.resolvedHeading),
            "damped turn is independent of step subdivision");
    auto across = state;
    across.pose.orientation =
        math::axisAngle({0, 1, 0}, float(std::numbers::pi - .01));
    auto shortest = bounded.advance(
        across, {.forward = 1, .viewHeading = -std::numbers::pi + .01}, .1);
    require(near(shortest.resolvedHeading, -std::numbers::pi + .01),
            "turning crosses wrap through shortest arc");

    auto lookProps = LocomotionProps::outOfNowhere();
    lookProps.response = TurnResponse::Immediate;
    lookProps.freeLookReturn = FreeLookReturn::KeepReference;
    CharacterFacingController look{lookProps};
    look.advance(state, {.forward = 1}, .1);
    auto free = look.advance(
        state, {.forward = 1, .viewHeading = 1, .freeLook = true}, .1);
    require(near(free.desiredHeading, 0),
            "free look preserves steering reference");
    auto held = look.advance(state, {.forward = 1, .viewHeading = 1}, .1);
    require(near(held.desiredHeading, 0),
            "keep-reference release policy is explicit");
    auto aim = look.advance(
        state, {.right = 1, .viewHeading = 1, .aim = true, .freeLook = true},
        .1);
    require(near(aim.desiredHeading, 1) && length(aim.velocity.linear) > 0,
            "aim overrides free look without dropping movement input");
    auto reset = state;
    ++reset.discontinuity;
    require(near(look.advance(reset, {.forward = 1, .viewHeading = .5}, .1)
                     .desiredHeading,
                 .5),
            "teleport resets steering history");

    for (const auto props : std::array{
             responsive, LocomotionProps::outOfNowhere(), tank, strafe}) {
      CharacterFacingController controller{props};
      auto subject = state;
      const auto target = translated(state.pose.position, {3, 0, 3});
      for (unsigned i = 0; i < 1000; ++i) {
        auto delta = relativeTo(target, subject.pose.position);
        if (length(delta) < .1)
          break;
        auto intent = travelIntent(
            subject, normalized(delta) * std::min(3.0, length(delta) / .02), {},
            props, .02);
        subject = realizeMovement(subject,
                                  controller.advance(subject, intent, .02), .02)
                      .actual;
        ++subject.tick;
      }
      require(length(relativeTo(target, subject.pose.position)) < .1,
              "autonomous travel respects each locomotion profile and reaches "
              "its goal");
    }
    FrameSample frame{{space, 8, 1},
                      {8, 2},
                      1,
                      1,
                      1,
                      {{space, {1e6, 0, 0}}, {}},
                      {space, {2, 0, 0}, {0, 1, 0}}};
    auto riding = state;
    riding.pose.position.meters.z = 2;
    auto ridingRequest =
        exploration.advance(riding, {.forward = 1}, .1, &frame);
    auto ridingResult = realizeMovement(riding, ridingRequest, .1);
    require(near(ridingResult.actual.pose.position.meters.x, 1e6) &&
                near(ridingResult.actual.pose.position.meters.z, 2.3) &&
                near(ridingResult.actual.velocity.linear.x, 4.3) &&
                ridingResult.attachment,
            "kinematic frame travel avoids integrating inherited motion twice "
            "but reports point velocity");
    require(
        realizeMovement(state, request, .1, MovementRealization::Physics)
                    .status == MovementStatus::Unsupported &&
            realizeMovement(state, request, .1, MovementRealization::RootMotion)
                    .actual.pose == state.pose,
        "future movement backends explicitly refuse realization without "
        "changing actual state");
    auto old = request;
    ++old.tick;
    test::rejects([&] { realizeMovement(state, old, .1); },
                  "stale tick request rejected");
    test::rejects<std::length_error>(
        [&] { realizeMovement(state, request, 1); },
        "kinematic integration step is bounded");
    test::rejects([&] { exploration.advance(state, {.right = 2}, .1); },
                  "out-of-range analog input rejected");
    const auto face = math::lookRotation({1, 0, 0});
    require(near(rotate(face, {0, 0, 1}).x, 1),
            "shared look rotation preserves camera and body convention");
    const auto same =
        math::slerp(face, {-face.x, -face.y, -face.z, -face.w}, .5);
    require(near(rotate(same, {0, 0, 1}).x, 1),
            "shared rotation interpolation handles antipodal representation");
  });
}
