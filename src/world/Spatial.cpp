#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <world/Spatial.hpp>

namespace playground::world {
namespace {
double finite(double value) {
  if (!std::isfinite(value))
    throw std::overflow_error("Spatial arithmetic exceeds finite range");
  return value;
}

Vec3d checked(Vec3d value) {
  if (!isFinite(value))
    throw std::invalid_argument("Spatial vector must be finite");
  return value;
}

void sameSpace(SpaceId a, SpaceId b) {
  validate(a);
  validate(b);
  if (a != b)
    throw std::invalid_argument("Spatial values belong to different spaces");
}

double largest(Vec3d v) {
  return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

float localFloat(double value, const SpatialLimits &limits) {
  if (!std::isfinite(value) || std::abs(value) > limits.maximumLocalCoordinate)
    throw std::out_of_range("Position exceeds render working extent");
  const auto result = static_cast<float>(value);
  if (std::abs(double(result) - value) > limits.localTolerance)
    throw std::out_of_range("Position exceeds render precision tolerance");
  return result;
}

void validOrigin(RenderOrigin origin, const SpatialLimits &limits) {
  limits.validate(origin.position);
  if (!origin.revision)
    throw std::invalid_argument("Render origin requires a revision");
}

std::int64_t cellIndex(double value) {
  const double integral = std::floor(finite(value));
  // int64 max rounds up to 2^63 in double, so the upper bound is exclusive.
  constexpr double end = 0x1p63;
  if (integral < -end || integral >= end)
    throw std::out_of_range("Cell coordinate exceeds integer range");
  return static_cast<std::int64_t>(integral);
}
} // namespace

bool isFinite(Vec3d v) noexcept {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3d operator+(Vec3d a, Vec3d b) {
  checked(a);
  checked(b);
  return {finite(a.x + b.x), finite(a.y + b.y), finite(a.z + b.z)};
}

Vec3d operator-(Vec3d a, Vec3d b) { return a + b * -1; }

Vec3d operator*(Vec3d a, double b) {
  checked(a);
  if (!std::isfinite(b))
    throw std::invalid_argument("Spatial multiplier must be finite");
  return {finite(a.x * b), finite(a.y * b), finite(a.z * b)};
}

double dot(Vec3d a, Vec3d b) {
  checked(a);
  checked(b);
  return finite(a.x * b.x + a.y * b.y + a.z * b.z);
}

Vec3d cross(Vec3d a, Vec3d b) {
  checked(a);
  checked(b);
  return {finite(a.y * b.z - a.z * b.y), finite(a.z * b.x - a.x * b.z),
          finite(a.x * b.y - a.y * b.x)};
}

double length(Vec3d a) {
  checked(a);
  return finite(std::hypot(a.x, a.y, a.z));
}

Vec3d normalized(Vec3d a) {
  checked(a);
  const double scale = largest(a);
  if (scale == 0)
    throw std::invalid_argument("Cannot normalize a zero direction");
  a = {a.x / scale, a.y / scale, a.z / scale};
  return a * (1 / length(a));
}

Vec3d rotate(math::Quaternion q, Vec3d value) {
  checked(value);
  q = math::normalizedRotation(q);
  const Vec3d u{q.x, q.y, q.z};
  // Correct the residual normalization error without narrowing the position.
  const double norm2 = dot(u, u) + double(q.w) * q.w;
  return value + cross(u, cross(u, value) + value * q.w) * (2 / norm2);
}

Vec3d inverseRotate(math::Quaternion q, Vec3d value) {
  return rotate({-q.x, -q.y, -q.z, q.w}, value);
}

void validate(SpaceId id) {
  if (!id.world.value || !id.value)
    throw std::invalid_argument("Space identity must be world-qualified");
}

void validate(EntityId id) {
  if (!id.world.value || !id.value)
    throw std::invalid_argument("Entity identity must be world-qualified");
}

void SpatialLimits::validate() const {
  for (double v : {maximumCoordinate, positionTolerance, maximumLocalCoordinate,
                   localTolerance})
    if (!std::isfinite(v) || v <= 0)
      throw std::invalid_argument("Spatial limits must be finite and positive");
  if (maximumLocalCoordinate > std::numeric_limits<float>::max())
    throw std::invalid_argument("Local extent exceeds float range");
  const double worldStep =
      std::nextafter(maximumCoordinate, INFINITY) - maximumCoordinate;
  const float localEnd = static_cast<float>(maximumLocalCoordinate);
  const double localStep =
      double(std::nextafter(localEnd, INFINITY)) - localEnd;
  if (worldStep > positionTolerance || localStep > localTolerance)
    throw std::invalid_argument("Spatial bounds exceed their precision budget");
}

void SpatialLimits::validate(WorldPosition p) const {
  validate();
  world::validate(p.space);
  checked(p.meters);
  if (largest(p.meters) > maximumCoordinate)
    throw std::out_of_range("Position exceeds world bounds");
}

void validate(WorldPose p, const SpatialLimits &limits) {
  limits.validate(p.position);
  math::normalizedRotation(p.orientation);
}

void validate(WorldVelocity v) {
  validate(v.space);
  checked(v.linear);
  checked(v.angular);
}

Vec3d relativeTo(WorldPosition p, WorldPosition origin) {
  sameSpace(p.space, origin.space);
  return p.meters - origin.meters;
}

WorldPosition translated(WorldPosition p, Vec3d delta,
                         const SpatialLimits &limits) {
  limits.validate(p);
  WorldPosition result{p.space, p.meters + delta};
  limits.validate(result);
  return result;
}

math::Vec3f renderPosition(WorldPosition p, RenderOrigin origin,
                           const SpatialLimits &limits) {
  limits.validate(p);
  validOrigin(origin, limits);
  const auto v = relativeTo(p, origin.position);
  return {localFloat(v.x, limits), localFloat(v.y, limits),
          localFloat(v.z, limits)};
}

WorldPosition worldPosition(math::Vec3f p, RenderOrigin origin,
                            const SpatialLimits &limits) {
  validOrigin(origin, limits);
  if (!math::isFinite(p) ||
      largest({p.x, p.y, p.z}) > limits.maximumLocalCoordinate)
    throw std::out_of_range("Local position exceeds render working extent");
  return translated(origin.position, {p.x, p.y, p.z}, limits);
}

WorldPose compose(WorldPose parent, LocalPose local,
                  const SpatialLimits &limits) {
  validate(parent, limits);
  return {translated(parent.position, rotate(parent.orientation, local.offset),
                     limits),
          parent.orientation * local.orientation};
}

LocalPose relativePose(WorldPose p, WorldPose parent,
                       const SpatialLimits &limits) {
  validate(p, limits);
  validate(parent, limits);
  const auto q = math::normalizedRotation(parent.orientation);
  return {inverseRotate(q, relativeTo(p.position, parent.position)),
          math::Quaternion{-q.x, -q.y, -q.z, q.w} * p.orientation};
}

WorldVelocity attachedVelocity(WorldPose parent, WorldVelocity velocity,
                               Vec3d localOffset, Vec3d localVelocity) {
  validate(velocity);
  sameSpace(parent.position.space, velocity.space);
  checked(parent.position.meters);
  const auto offset = rotate(parent.orientation, localOffset);
  return {velocity.space,
          velocity.linear + cross(velocity.angular, offset) +
              rotate(parent.orientation, localVelocity),
          velocity.angular};
}

GridCoordinate cellAt(WorldPosition p, WorldPosition origin,
                      double cellMeters) {
  if (!std::isfinite(cellMeters) || cellMeters <= 0)
    throw std::invalid_argument("Cell size must be finite positive meters");
  const auto v = relativeTo(p, origin);
  return {cellIndex(v.x / cellMeters), cellIndex(v.y / cellMeters),
          cellIndex(v.z / cellMeters)};
}

ChunkAxis splitBlock(std::int64_t block, std::uint32_t extent) {
  if (!extent)
    throw std::invalid_argument("Chunk extent must be positive");
  const auto divisor = std::int64_t(extent);
  auto chunk = block / divisor;
  auto local = block % divisor;
  if (local < 0) {
    --chunk;
    local += divisor;
  }
  return {chunk, static_cast<std::uint32_t>(local)};
}

std::int64_t joinBlock(ChunkAxis axis, std::uint32_t extent) {
  if (!extent || axis.local >= extent)
    throw std::invalid_argument("Invalid chunk-local block coordinate");
  const auto first =
      splitBlock(std::numeric_limits<std::int64_t>::min(), extent);
  const auto last =
      splitBlock(std::numeric_limits<std::int64_t>::max(), extent);
  if (axis.chunk < first.chunk || axis.chunk > last.chunk ||
      (axis.chunk == first.chunk && axis.local < first.local) ||
      (axis.chunk == last.chunk && axis.local > last.local))
    throw std::out_of_range("Block coordinate exceeds integer range");
  if (axis.chunk == first.chunk)
    return std::numeric_limits<std::int64_t>::min() +
           (axis.local - first.local);
  return axis.chunk * std::int64_t(extent) + axis.local;
}

} // namespace playground::world
