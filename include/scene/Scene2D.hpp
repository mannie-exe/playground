#pragma once

#include <map>
#include <optional>
#include <vector>

#include <rendering/PaintImage.hpp>

namespace playground::scene {

// Ordered world content, not UI layout or an event dispatcher. No depth buffer:
// ascending zOrder, then creation order defines translucent composition.
struct Item2DProps {
  math::Rect bounds;
  math::Transform2D transform;
  rendering::PaintImageHandle image;
  rendering::ImagePaint paint;
  int zOrder{};
  bool visible{true};
};
struct Item2DId {
  std::uint64_t owner{}, value{};
  bool operator==(const Item2DId &) const = default;
};
struct Item2DPatch {
  std::optional<math::Rect> bounds;
  std::optional<math::Transform2D> transform;
  std::optional<rendering::PaintImageHandle> image;
  std::optional<rendering::ImagePaint> paint;
  std::optional<int> zOrder;
  std::optional<bool> visible;
};
class Scene2D {
  std::uint64_t _owner, _next{1}, _revision{};
  std::map<std::uint64_t, Item2DProps> _items;

public:
  Scene2D();
  Scene2D(const Scene2D &) = delete;
  Scene2D &operator=(const Scene2D &) = delete;
  Item2DId create(Item2DProps props);
  bool contains(Item2DId id) const noexcept;
  const Item2DProps &props(Item2DId id) const;
  void setProps(Item2DId id, Item2DProps props);
  void applyPatch(Item2DId id, const Item2DPatch &patch);
  void remove(Item2DId id);
  std::vector<Item2DProps> snapshot() const;
  std::uint64_t revision() const noexcept { return _revision; }
};

} // namespace playground::scene
