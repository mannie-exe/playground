#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include <scene/WorldScene.hpp>

namespace playground::scene {
namespace {
std::size_t bytes(std::size_t count, std::size_t element,
                  std::size_t base = 0) {
  if (count > (std::numeric_limits<std::size_t>::max() - base) / element)
    throw std::length_error("World scene storage exceeds addressable range");
  return base + count * element;
}
} // namespace

CameraProps WorldCamera::localCamera(world::RenderOrigin origin,
                                     const world::SpatialLimits &limits) const {
  world::validate(pose, limits);
  lens.view(1);
  if (!std::isfinite(focusDistance) || focusDistance <= 0)
    throw std::invalid_argument("World camera focus distance must be positive");
  auto result = lens;
  result.eye = world::renderPosition(pose.position, origin, limits);
  const auto forward = world::rotate(pose.orientation, {0, 0, 1});
  const auto up = world::rotate(pose.orientation, {0, 1, 0});
  result.target = result.eye + math::Vec3f{float(forward.x), float(forward.y),
                                           float(forward.z)};
  result.up = {float(up.x), float(up.y), float(up.z)};
  result.view(1);
  return result;
}

WorldSceneSnapshot::WorldSceneSnapshot(world::WorldSnapshot state)
    : _world{std::move(state)} {}

CameraView WorldCamera::view(world::RenderOrigin origin,
                             const world::SpatialLimits &limits,
                             float aspect) const {
  const auto local = localCamera(origin, limits);
  auto result = local.view(aspect);
  const auto rotation = math::rotation(pose.orientation);
  const auto eye = world::relativeTo(pose.position, origin.position);
  // Build inverse rigid pose directly; eye + tiny direction loses orientation
  // precision for a non-camera-centered working origin.
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c)
      result.view.at(r, c) = rotation.at(c, r);
    const double offset = -(double(rotation.at(0, r)) * eye.x +
                            double(rotation.at(1, r)) * eye.y +
                            double(rotation.at(2, r)) * eye.z);
    if (!std::isfinite(offset) ||
        std::abs(offset) > std::numeric_limits<float>::max())
      throw std::out_of_range("Camera translation exceeds local matrix range");
    result.view.at(r, 3) = float(offset);
  }
  return result;
}

SceneProjection::SceneProjection(
    std::vector<EntityVisual> visuals,
    std::shared_ptr<rendering::ResourceLedger> ledger, std::size_t maxVisuals)
    : _ledger{std::move(ledger)}, _visuals{std::move(visuals)} {
  if (!_ledger || !maxVisuals || _visuals.size() > maxVisuals)
    throw std::invalid_argument("Invalid world scene bindings or limits");
  for (const auto &v : _visuals) {
    world::validate(v.entity);
    if (!v.draw.mesh || !math::isFinite(v.draw.model) ||
        v.draw.model.at(3, 0) != 0 || v.draw.model.at(3, 1) != 0 ||
        v.draw.model.at(3, 2) != 0 || v.draw.model.at(3, 3) != 1)
      throw std::invalid_argument(
          "World visual requires an affine mesh binding");
    scene::validate(v.draw.material);
  }
  _charge = _ledger->reserve(
      rendering::MemoryClass::CPU, rendering::ResourceKind::Asset,
      bytes(_visuals.capacity(), sizeof(EntityVisual), sizeof(SceneProjection)),
      "World scene bindings");
  _charge->setState(rendering::AllocationState::Owned);
}

std::shared_ptr<const WorldSceneSnapshot>
SceneProjection::extract(world::WorldSnapshot state, const WorldCamera &camera,
                         world::RenderOrigin origin) const {
  if (const auto last = _last.lock();
      last && last->world().version() == state.version() &&
      last->camera() == camera && last->origin() == origin)
    return last;
  const auto *space = state.space(origin.position.space);
  if (!space)
    throw std::invalid_argument(
        "Render origin space is absent from world snapshot");
  camera.view(origin, space->limits, 1);
  auto charge = _ledger->reserve(
      rendering::MemoryClass::CPU, rendering::ResourceKind::Preparation,
      bytes(_visuals.size(), sizeof(MeshDraw) + sizeof(world::EntityId),
            sizeof(WorldSceneSnapshot)),
      "World scene extraction");
  auto result = std::shared_ptr<WorldSceneSnapshot>(
      new WorldSceneSnapshot{std::move(state)});
  result->_charge = std::move(charge);
  result->_origin = origin;
  result->_limits = space->limits;
  result->_camera = camera;
  result->_draws.reserve(_visuals.size());
  result->_entities.reserve(_visuals.size());
  for (const auto &visual : _visuals) {
    const auto *entity = result->_world.find(visual.entity);
    if (!entity || entity->destroyed ||
        entity->props.pose.position.space != origin.position.space)
      continue;
    const auto &pose = entity->props.pose;
    // Compose the local translation in double precision before origin
    // subtraction.
    const auto &m = visual.draw.model;
    auto draw = visual.draw;
    try {
      const auto position = world::translated(
          pose.position,
          world::rotate(pose.orientation, {m.at(0, 3), m.at(1, 3), m.at(2, 3)}),
          space->limits);
      const auto local = world::renderPosition(position, origin, space->limits);
      auto basis = m;
      basis.at(0, 3) = basis.at(1, 3) = basis.at(2, 3) = 0;
      draw.model =
          math::translation(local) * math::rotation(pose.orientation) * basis;
      if (!math::isFinite(draw.model))
        throw std::out_of_range("Visual basis exceeds local matrix range");
      // The entity's anchor can fit while scaled/offset geometry does not.
      // Check all affine bounds corners before admitting a float submission.
      const auto &bounds = draw.mesh->bounds();
      for (unsigned corner = 0; corner < 8; ++corner) {
        const math::Vec3f vertex{
            corner & 1 ? bounds.maximum.x : bounds.minimum.x,
            corner & 2 ? bounds.maximum.y : bounds.minimum.y,
            corner & 4 ? bounds.maximum.z : bounds.minimum.z};
        const world::Vec3d offset{
            double(m.at(0, 0)) * vertex.x + double(m.at(0, 1)) * vertex.y +
                double(m.at(0, 2)) * vertex.z + m.at(0, 3),
            double(m.at(1, 0)) * vertex.x + double(m.at(1, 1)) * vertex.y +
                double(m.at(1, 2)) * vertex.z + m.at(1, 3),
            double(m.at(2, 0)) * vertex.x + double(m.at(2, 1)) * vertex.y +
                double(m.at(2, 2)) * vertex.z + m.at(2, 3)};
        const auto precise = world::translated(
            pose.position, world::rotate(pose.orientation, offset),
            space->limits);
        world::renderPosition(precise, origin, space->limits);
        const auto expected = world::relativeTo(precise, origin.position);
        const auto actual = math::transformPoint(draw.model, vertex);
        if (!math::isFinite(actual) ||
            std::abs(double(actual.x) - expected.x) >
                space->limits.localTolerance ||
            std::abs(double(actual.y) - expected.y) >
                space->limits.localTolerance ||
            std::abs(double(actual.z) - expected.z) >
                space->limits.localTolerance)
          throw std::out_of_range(
              "Visual bounds exceed local precision tolerance");
      }
    } catch (const std::out_of_range &) {
      ++result->_omitted;
      continue;
    }
    result->_draws.push_back(std::move(draw));
    result->_entities.push_back(entity->id);
  }
  result->_charge->setState(rendering::AllocationState::Owned);
  _last = result;
  return result;
}

} // namespace playground::scene
