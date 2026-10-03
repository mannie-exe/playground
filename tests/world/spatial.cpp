#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <support/Test.hpp>
#include <world/Spatial.hpp>

using namespace playground;
using namespace playground::world;
using test::require;

int main() {
  return test::run([] {
    const SpaceId space{{1}, 1}, other{{1}, 2}, foreign{{2}, 1};
    SpatialLimits limits;
    limits.validate();
    for (double offset : {0., 1e6, -1e6}) {
      const RenderOrigin origin{{space, {offset, offset, offset}}, 1};
      const WorldPosition p{space, {offset + .02, offset - .03, offset + 1}};
      const auto local = renderPosition(p, origin);
      require(std::abs(local.x - .02) < 1e-8 &&
                  std::abs(local.y + .03) < 1e-8 && local.z == 1,
              "small offsets survive distant positive and negative origins");
      const auto restored = worldPosition(local, origin);
      require(length(relativeTo(restored, p)) < 1e-8,
              "render position round trips using the captured origin");
      const auto shifted =
          renderPosition(p, {{space, {offset + 1, offset, offset}}, 2});
      require(std::abs(shifted.x + .98) < 1e-7,
              "rebasing changes only local representation");
    }
    test::rejects([&] { relativeTo({space, {}}, {other, {}}); },
                  "foreign space rejected");
    test::rejects([&] { relativeTo({space, {}}, {foreign, {}}); },
                  "foreign world rejected");
    test::rejects([&] { validate(SpaceId{}); }, "null space rejected");
    test::rejects([&] { validate(EntityId{}); }, "null entity rejected");
    test::rejects([&] { limits.validate({space, {NAN, 0, 0}}); },
                  "nonfinite position rejected");
    test::rejects<std::out_of_range>(
        [&] { limits.validate({space, {1e10, 0, 0}}); },
        "world bounds enforced");
    test::rejects([&] { SpatialLimits{.maximumCoordinate = 1e20}.validate(); },
                  "impossible double precision budget rejected");
    test::rejects([&] { SpatialLimits{.localTolerance = 1e-9}.validate(); },
                  "impossible local precision budget rejected");
    test::rejects(
        [&] { SpatialLimits{.maximumCoordinate = INFINITY}.validate(); },
        "nonfinite bounds rejected");
    test::rejects([&] { renderPosition({space, {}}, {{space, {}}, 0}); },
                  "unversioned origin rejected");
    test::rejects<std::out_of_range>(
        [&] { renderPosition({space, {9000, 0, 0}}, {{space, {}}, 1}); },
        "local extent enforced");
    test::rejects<std::out_of_range>(
        [&] { worldPosition({INFINITY, 0, 0}, {{space, {}}, 1}); },
        "nonfinite local input rejected");
    const double maximum = std::numeric_limits<double>::max();
    test::rejects<std::overflow_error>(
        [&] { (void)(Vec3d{maximum, 0, 0} + Vec3d{maximum, 0, 0}); },
        "arithmetic overflow rejected");
    require(std::abs(length(normalized({maximum, maximum, maximum})) - 1) <
                1e-15,
            "large directions normalize without overflow");
    test::rejects([&] { normalized({}); }, "zero direction rejected");

    const auto quarter =
        math::axisAngle({0, 1, 0}, std::numbers::pi_v<float> / 2);
    const WorldPose parent{{space, {1e6, 5, -1e6}}, quarter};
    const LocalPose local{{.02, 0, 2}, math::axisAngle({1, 0, 0}, .2f)};
    const auto child = compose(parent, local);
    const auto relative = relativePose(child, parent);
    require(length(relative.offset - local.offset) < 1e-8,
            "frame composition preserves local position at distant origins");
    require(length(rotate(relative.orientation, {0, 0, 1}) -
                   rotate(local.orientation, {0, 0, 1})) < 1e-6,
            "relative rotation composes independently of translation");
    const auto v = attachedVelocity(
        {{space, {}}, {}}, {space, {1, 0, 0}, {0, 2, 0}}, {0, 0, 3}, {0, 0, 4});
    require(v.linear == Vec3d{7, 0, 4} && v.angular == Vec3d{0, 2, 0},
            "attached velocity includes rotation about the parent origin");
    test::rejects([&] { attachedVelocity(parent, {foreign, {}, {}}, {}); },
                  "foreign frame velocity rejected");
    test::rejects([&] { compose(parent, {{}, {0, 0, 0, 0}}); },
                  "invalid frame rotation rejected");

    require(cellAt({space, {0, -0.01, -16}}, {space, {}}, 16) ==
                GridCoordinate{0, -1, -1},
            "cell partition uses half-open floor intervals");
    require(cellAt({space, {16, -16.01, 32}}, {space, {}}, 16) ==
                GridCoordinate{1, -2, 2},
            "cell boundaries have unique membership");
    test::rejects([&] { cellAt({space, {}}, {space, {}}, 0); },
                  "zero cell extent rejected");
    test::rejects<std::out_of_range>(
        [&] { cellAt({space, {0x1p63, 0, 0}}, {space, {}}, 1); },
        "unrepresentable positive cell index rejected");
    for (auto extent : {1u, 3u, 16u, std::numeric_limits<std::uint32_t>::max()})
      for (std::int64_t block :
           {std::numeric_limits<std::int64_t>::min(), std::int64_t{-33},
            std::int64_t{-1}, std::int64_t{0}, std::int64_t{32},
            std::numeric_limits<std::int64_t>::max()}) {
        const auto parts = splitBlock(block, extent);
        require(parts.local < extent && joinBlock(parts, extent) == block,
                "chunk addressing round trips across signed integer range");
      }
    require(splitBlock(-1, 16) == ChunkAxis{-1, 15},
            "negative block local coordinate is positive");
    test::rejects([&] { splitBlock(0, 0); }, "zero chunk extent rejected");
    test::rejects([&] { joinBlock({0, 16}, 16); },
                  "invalid local block rejected");
    test::rejects<std::out_of_range>(
        [&] { joinBlock({std::numeric_limits<std::int64_t>::max(), 0}, 16); },
        "positive block overflow rejected");
    test::rejects<std::out_of_range>(
        [&] { joinBlock({std::numeric_limits<std::int64_t>::min(), 0}, 16); },
        "negative block overflow rejected");
  });
}
