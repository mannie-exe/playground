#include <algorithm>
#include <atomic>
#include <stdexcept>

#include <scene/Scene2D.hpp>

namespace playground::scene {
namespace {
std::atomic<std::uint64_t> nextOwner{1};

void validate(const Item2DProps &props) {
  if (props.paint.sampling != rendering::Sampling::Nearest &&
      props.paint.sampling != rendering::Sampling::Linear)
    throw std::invalid_argument("Invalid 2D scene sampling mode");
  if (!math::isFinite(props.bounds) || !math::isNonNegative(props.bounds.size))
    throw std::invalid_argument("Invalid 2D scene bounds");
  for (float value :
       {props.transform.a, props.transform.b, props.transform.c,
        props.transform.d, props.transform.tx, props.transform.ty})
    if (!std::isfinite(value))
      throw std::invalid_argument("Invalid 2D scene transform");
  if (props.image && (!math::isFinite(props.image->pixelSize()) ||
                      !math::hasArea(props.image->pixelSize())))
    throw std::invalid_argument("Invalid 2D scene image");
}
} // namespace

Scene2D::Scene2D() : _owner{nextOwner.fetch_add(1)} {
  if (!_owner)
    throw std::overflow_error("Scene identity exhausted");
}

Item2DId Scene2D::create(Item2DProps props) {
  validate(props);
  if (!_next)
    throw std::overflow_error("Scene item identity exhausted");
  const auto id = _next;
  _items.emplace(id, std::move(props));
  ++_next;
  ++_revision;
  return {_owner, id};
}

bool Scene2D::contains(Item2DId id) const noexcept {
  return id.owner == _owner && _items.contains(id.value);
}

const Item2DProps &Scene2D::props(Item2DId id) const {
  if (!contains(id))
    throw std::invalid_argument("Stale or foreign scene item");
  return _items.at(id.value);
}

void Scene2D::setProps(Item2DId id, Item2DProps props) {
  this->props(id);
  validate(props);
  _items.at(id.value) = std::move(props);
  ++_revision;
}

void Scene2D::remove(Item2DId id) {
  props(id);
  _items.erase(id.value);
  ++_revision;
}

void Scene2D::applyPatch(Item2DId id, const Item2DPatch &patch) {
  auto value = props(id);
  if (patch.bounds)
    value.bounds = *patch.bounds;
  if (patch.transform)
    value.transform = *patch.transform;
  if (patch.image)
    value.image = *patch.image;
  if (patch.paint)
    value.paint = *patch.paint;
  if (patch.zOrder)
    value.zOrder = *patch.zOrder;
  if (patch.visible)
    value.visible = *patch.visible;
  setProps(id, std::move(value));
}

std::vector<Item2DProps> Scene2D::snapshot() const {
  std::vector<Item2DProps> result;
  for (const auto &[id, item] : _items)
    if (item.visible)
      result.push_back(item);
  std::stable_sort(
      result.begin(), result.end(),
      [](const auto &a, const auto &b) { return a.zOrder < b.zOrder; });
  return result;
}
} // namespace playground::scene
