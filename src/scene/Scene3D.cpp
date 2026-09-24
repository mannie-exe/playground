#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

#include <scene/Scene3D.hpp>

namespace playground::scene {
namespace {
std::atomic<std::uint64_t> nextOwner{1};
void validateProps(const ObjectProps &props) {
  props.transform.matrix();
  validate(props.material);
  if (props.mesh)
    validate({{}, {1, 1}, {}},
             std::array{MeshDraw{props.mesh, props.material, {}}});
}
} // namespace

CameraView CameraProps::view(float aspect) const {
  return {math::lookAtLH(eye, target, up),
          orthographicHeight
              ? math::orthographicLH(*orthographicHeight * aspect,
                                     *orthographicHeight, nearPlane, farPlane)
              : math::perspectiveLH(verticalFov, aspect, nearPlane, farPlane)};
}

Scene3D::Scene3D() : _owner{nextOwner.fetch_add(1)} {
  if (!_owner)
    throw std::overflow_error("Scene identity exhausted");
}

bool Scene3D::contains(ObjectId id) const noexcept {
  return id.owner == _owner && id.index < _entries.size() &&
         _entries[id.index].generation == id.generation &&
         _entries[id.index].props.has_value();
}

const Scene3D::Entry &Scene3D::entry(ObjectId id) const {
  if (!contains(id))
    throw std::invalid_argument("Stale or foreign scene object");
  return _entries[id.index];
}
Scene3D::Entry &Scene3D::entry(ObjectId id) {
  return const_cast<Entry &>(std::as_const(*this).entry(id));
}

ObjectId Scene3D::create(ObjectProps props, std::optional<ObjectId> parent) {
  validateProps(props);
  if (parent)
    entry(*parent).children.reserve(entry(*parent).children.size() + 1);
  if (!_free.empty()) {
    const auto i = _free.back();
    auto &slot = _entries[i];
    slot.props = std::move(props);
    slot.parent = parent;
    const ObjectId id{_owner, static_cast<std::uint32_t>(i), slot.generation};
    if (parent)
      entry(*parent).children.push_back(id);
    _free.pop_back();
    ++_revision;
    return id;
  }
  if (_entries.size() >= std::numeric_limits<std::uint32_t>::max())
    throw std::length_error("Too many scene objects");
  _entries.push_back({std::move(props), parent});
  const ObjectId id{_owner, static_cast<std::uint32_t>(_entries.size() - 1), 1};
  if (parent)
    entry(*parent).children.push_back(id);
  ++_revision;
  return id;
}

const ObjectProps &Scene3D::props(ObjectId id) const {
  return *entry(id).props;
}
std::vector<ObjectId>
Scene3D::createBatch(std::span<const ObjectProps> objects,
                     std::span<const std::optional<std::size_t>> parents,
                     std::optional<ObjectId> parent) {
  if (objects.size() != parents.size())
    throw std::invalid_argument("Scene batch parent count mismatch");
  if (parent)
    entry(*parent);
  if (objects.size() >
      std::numeric_limits<std::uint32_t>::max() - _entries.size())
    throw std::length_error("Scene batch exceeds object capacity");
  for (std::size_t i = 0; i < objects.size(); ++i) {
    validateProps(objects[i]);
    if (parents[i] && *parents[i] >= i)
      throw std::invalid_argument("Scene batch parent must precede child");
  }
  if (objects.empty())
    return {};
  auto entries = _entries;
  entries.reserve(entries.size() + objects.size());
  std::vector<ObjectId> result;
  result.reserve(objects.size());
  for (std::size_t i = 0; i < objects.size(); ++i) {
    const auto parentId =
        parents[i] ? std::optional{result[*parents[i]]} : parent;
    const ObjectId id{_owner, static_cast<std::uint32_t>(entries.size()), 1};
    entries.push_back({objects[i], parentId});
    if (parentId)
      entries[parentId->index].children.push_back(id);
    result.push_back(id);
  }
  _entries.swap(entries);
  ++_revision;
  return result;
}
void Scene3D::setProps(ObjectId id, ObjectProps props) {
  entry(id);
  validateProps(props);
  entry(id).props = std::move(props);
  ++_revision;
}
void Scene3D::applyPatch(ObjectId id, const ObjectPatch &patch) {
  auto value = props(id);
  if (patch.transform)
    value.transform = *patch.transform;
  if (patch.mesh)
    value.mesh = *patch.mesh;
  if (patch.material)
    value.material = *patch.material;
  if (patch.visible)
    value.visible = *patch.visible;
  setProps(id, std::move(value));
}
void Scene3D::setParent(ObjectId id, std::optional<ObjectId> parent) {
  entry(id);
  for (auto ancestor = parent; ancestor; ancestor = entry(*ancestor).parent)
    if (*ancestor == id)
      throw std::invalid_argument("Scene parenting cycle");
  if (parent == entry(id).parent)
    return;
  if (parent)
    entry(*parent).children.reserve(entry(*parent).children.size() + 1);
  if (const auto old = entry(id).parent)
    std::erase(entry(*old).children, id);
  if (parent)
    entry(*parent).children.push_back(id);
  entry(id).parent = parent;
  ++_revision;
}

void Scene3D::remove(ObjectId id) {
  entry(id);
  std::vector<ObjectId> removed{id};
  for (std::size_t cursor = 0; cursor < removed.size(); ++cursor)
    for (const auto child : entry(removed[cursor]).children)
      removed.push_back(child);
  // Allocate the work list before modifying any objects.
  _free.reserve(_free.size() + removed.size());
  if (const auto parent = entry(id).parent)
    std::erase(entry(*parent).children, id);
  for (auto object : removed) {
    auto &slot = entry(object);
    slot.props.reset();
    slot.parent.reset();
    slot.children.clear();
    ++slot.generation;
    if (slot.generation != std::numeric_limits<std::uint32_t>::max())
      _free.push_back(object.index);
  }
  ++_revision;
}

math::Matrix4 Scene3D::worldTransform(ObjectId id) const {
  entry(id);
  refreshWorldCache();
  return entry(id).world;
}

void Scene3D::refreshWorldCache() const {
  if (_cachedRevision == _revision)
    return;
  std::vector<bool> ready(_entries.size());
  std::vector<std::size_t> chain;
  std::vector<MeshDraw> draws;
  for (std::size_t i = 0; i < _entries.size(); ++i) {
    if (!_entries[i].props || ready[i])
      continue;
    chain.clear();
    for (std::size_t current = i; !ready[current];) {
      chain.push_back(current);
      if (!_entries[current].parent)
        break;
      current = _entries[current].parent->index;
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      auto &slot = _entries[*it];
      slot.world = slot.props->transform.matrix();
      slot.worldVisible = slot.props->visible;
      if (slot.parent) {
        const auto &parent = _entries[slot.parent->index];
        slot.world = parent.world * slot.world;
        slot.worldVisible = parent.worldVisible && slot.worldVisible;
      }
      if (!math::isFinite(slot.world))
        throw std::overflow_error("Scene world transform overflow");
      ready[*it] = true;
    }
  }
  for (std::size_t i = 0; i < _entries.size(); ++i) {
    const auto &slot = _entries[i];
    if (!slot.props || !slot.props->mesh)
      continue;
    if (slot.worldVisible)
      draws.push_back({slot.props->mesh, slot.props->material, slot.world});
  }
  _cachedDraws = std::move(draws);
  _cachedRevision = _revision;
}

std::vector<MeshDraw> Scene3D::snapshot() const {
  refreshWorldCache();
  return _cachedDraws;
}

Ray3 pickingRay(const CameraView &camera, math::Vec2f position) {
  if (!math::isFinite(position))
    throw std::invalid_argument("Nonfinite viewport position");
  const auto unproject = math::inverse(camera.projection * camera.view);
  const float x = position.x * 2 - 1, y = 1 - position.y * 2;
  const auto near = math::transformPoint(unproject, {x, y, 0});
  const auto far = math::transformPoint(unproject, {x, y, 1});
  return {near, math::normalized(far - near)};
}

Bounds3 meshBounds(const Mesh &mesh) { return mesh.bounds(); }

std::optional<float> intersect(Ray3 ray, Bounds3 bounds) {
  if (!math::isFinite(ray.origin) || !math::isFinite(bounds.minimum) ||
      !math::isFinite(bounds.maximum) || bounds.minimum.x > bounds.maximum.x ||
      bounds.minimum.y > bounds.maximum.y ||
      bounds.minimum.z > bounds.maximum.z)
    throw std::invalid_argument("Invalid ray or bounds");
  ray.direction = math::normalized(ray.direction);
  double near = 0, far = std::numeric_limits<double>::infinity();
  const std::array origin{ray.origin.x, ray.origin.y, ray.origin.z},
      direction{ray.direction.x, ray.direction.y, ray.direction.z};
  const std::array minimum{bounds.minimum.x, bounds.minimum.y,
                           bounds.minimum.z},
      maximum{bounds.maximum.x, bounds.maximum.y, bounds.maximum.z};
  for (int i = 0; i < 3; ++i) {
    if (direction[i] == 0) {
      if (origin[i] < minimum[i] || origin[i] > maximum[i])
        return {};
    } else {
      double a = (double(minimum[i]) - origin[i]) / direction[i],
             b = (double(maximum[i]) - origin[i]) / direction[i];
      if (a > b)
        std::swap(a, b);
      near = std::max(near, a);
      far = std::min(far, b);
      if (near > far)
        return {};
    }
  }
  if (near > std::numeric_limits<float>::max())
    return {};
  return float(near);
}

std::optional<PickResult> Scene3D::pick(Ray3 ray) const {
  if (!math::isFinite(ray.origin))
    throw std::invalid_argument("Nonfinite picking origin");
  ray.direction = math::normalized(ray.direction);
  refreshWorldCache();
  std::optional<PickResult> closest;
  for (std::size_t i = 0; i < _entries.size(); ++i) {
    const auto &slot = _entries[i];
    if (!slot.props || !slot.worldVisible || !slot.props->mesh)
      continue;
    const ObjectId id{_owner, static_cast<std::uint32_t>(i), slot.generation};
    const auto model = worldTransform(id);
    const auto local = slot.props->mesh->bounds();
    Bounds3 bounds{math::transformPoint(model, local.minimum),
                   math::transformPoint(model, local.minimum)};
    for (int corner = 0; corner < 8; ++corner) {
      const auto point = math::transformPoint(
          model, {corner & 1 ? local.maximum.x : local.minimum.x,
                  corner & 2 ? local.maximum.y : local.minimum.y,
                  corner & 4 ? local.maximum.z : local.minimum.z});
      bounds.minimum = {std::min(bounds.minimum.x, point.x),
                        std::min(bounds.minimum.y, point.y),
                        std::min(bounds.minimum.z, point.z)};
      bounds.maximum = {std::max(bounds.maximum.x, point.x),
                        std::max(bounds.maximum.y, point.y),
                        std::max(bounds.maximum.z, point.z)};
    }
    const auto distance = intersect(ray, bounds);
    if (!distance || (closest && *distance > closest->distance))
      continue;
    const auto &mesh = slot.props->mesh->data();
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const auto a =
          math::transformPoint(model, mesh.vertices[mesh.indices[t]].position);
      const auto b = math::transformPoint(
          model, mesh.vertices[mesh.indices[t + 1]].position);
      const auto c = math::transformPoint(
          model, mesh.vertices[mesh.indices[t + 2]].position);
      const auto ab = b - a, ac = c - a, p = math::cross(ray.direction, ac);
      const double determinant = math::dot(ab, p);
      if (std::abs(determinant) < 1e-10)
        continue;
      const auto s = ray.origin - a;
      const double u = math::dot(s, p) / determinant;
      if (u < 0 || u > 1)
        continue;
      const auto q = math::cross(s, ab);
      const double v = math::dot(ray.direction, q) / determinant;
      if (v < 0 || u + v > 1)
        continue;
      const double distance = math::dot(ac, q) / determinant;
      if (distance >= 0 && distance <= std::numeric_limits<float>::max() &&
          (!closest || distance < closest->distance))
        closest = PickResult{id, float(distance),
                             ray.origin + ray.direction * float(distance)};
    }
  }
  return closest;
}

} // namespace playground::scene
