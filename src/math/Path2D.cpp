#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <math/Path2D.hpp>

namespace playground::math {
namespace {
void finite(Point2 p) {
  if (!isFinite(p))
    throw std::invalid_argument("Path coordinates must be finite");
}
double distanceSquared(Point2 p, Point2 a, Point2 b) {
  const double dx = double(b.x) - a.x, dy = double(b.y) - a.y;
  const double length = dx * dx + dy * dy;
  const double t =
      length == 0
          ? 0
          : std::clamp(((double(p.x) - a.x) * dx + (double(p.y) - a.y) * dy) /
                           length,
                       0.0, 1.0);
  const double x = double(p.x) - a.x - t * dx;
  const double y = double(p.y) - a.y - t * dy;
  return x * x + y * y;
}
Point2 middle(Point2 a, Point2 b) {
  return {float((double(a.x) + b.x) / 2), float((double(a.y) + b.y) / 2)};
}
} // namespace

Path2D &Path2D::moveTo(Point2 point) {
  finite(point);
  _commands.push_back({PathVerb::Move, point, {}, {}});
  return *this;
}
Path2D &Path2D::lineTo(Point2 point) {
  finite(point);
  _commands.push_back({PathVerb::Line, point, {}, {}});
  return *this;
}
Path2D &Path2D::quadraticTo(Point2 control, Point2 end) {
  finite(control);
  finite(end);
  _commands.push_back({PathVerb::Quadratic, end, control, {}});
  return *this;
}
Path2D &Path2D::cubicTo(Point2 first, Point2 second, Point2 end) {
  finite(first);
  finite(second);
  finite(end);
  _commands.push_back({PathVerb::Cubic, end, first, second});
  return *this;
}
Path2D &Path2D::close() {
  _commands.push_back({PathVerb::Close, {}, {}, {}});
  return *this;
}

FlattenedPath flattenPath(const Path2D &path, PathFlattenProps props) {
  if (!std::isfinite(props.tolerance) || props.tolerance <= 0 ||
      !props.maximumSegments || !props.maximumDepth || props.maximumDepth > 24)
    throw std::invalid_argument("Invalid path flattening policy");
  FlattenedPath result;
  Point2 current{}, start{};
  bool active{}, hasBounds{};
  Point2 minimum{}, maximum{};
  const auto add = [&](Point2 from, Point2 to) {
    if (from == to)
      return;
    if (result.segments.size() == props.maximumSegments)
      throw std::length_error("Path exceeds its segment budget");
    result.segments.push_back({from, to});
    for (auto p : {from, to}) {
      if (!hasBounds) {
        minimum = maximum = p;
        hasBounds = true;
      }
      minimum = {std::min(minimum.x, p.x), std::min(minimum.y, p.y)};
      maximum = {std::max(maximum.x, p.x), std::max(maximum.y, p.y)};
    }
  };
  const double tolerance = double(props.tolerance) * props.tolerance;
  const auto cubic = [&](auto &&self, Point2 a, Point2 b, Point2 c, Point2 d,
                         unsigned depth) -> void {
    if (std::max(distanceSquared(b, a, d), distanceSquared(c, a, d)) <=
        tolerance) {
      add(a, d);
      return;
    }
    if (depth == props.maximumDepth)
      throw std::length_error("Path curve exceeds flattening depth");
    auto ab = middle(a, b), bc = middle(b, c), cd = middle(c, d);
    auto abc = middle(ab, bc), bcd = middle(bc, cd), center = middle(abc, bcd);
    self(self, a, ab, abc, center, depth + 1);
    self(self, center, bcd, cd, d, depth + 1);
  };
  for (const auto &command : path.commands()) {
    if (command.verb == PathVerb::Move) {
      if (active && props.closeOpenContours)
        add(current, start);
      current = start = command.end;
      active = true;
      continue;
    }
    if (!active)
      throw std::invalid_argument("Path must begin a contour with moveTo");
    switch (command.verb) {
    case PathVerb::Line:
      add(current, command.end);
      break;
    case PathVerb::Quadratic: {
      const auto mix = [](Point2 a, Point2 b) {
        return Point2{float(double(a.x) / 3 + 2 * double(b.x) / 3),
                      float(double(a.y) / 3 + 2 * double(b.y) / 3)};
      };
      cubic(cubic, current, mix(current, command.control1),
            mix(command.end, command.control1), command.end, 0);
      break;
    }
    case PathVerb::Cubic:
      cubic(cubic, current, command.control1, command.control2, command.end, 0);
      break;
    case PathVerb::Close:
      add(current, start);
      current = start;
      continue;
    default:
      throw std::invalid_argument("Unknown path command");
    }
    current = command.end;
  }
  if (active && props.closeOpenContours)
    add(current, start);
  if (hasBounds) {
    result.bounds = {minimum, {maximum.x - minimum.x, maximum.y - minimum.y}};
    if (!isFinite(result.bounds))
      throw std::overflow_error("Path bounds overflow");
  }
  return result;
}

bool FlattenedPath::contains(Point2 point, FillRule rule) const {
  if (rule != FillRule::NonZero && rule != FillRule::EvenOdd)
    throw std::invalid_argument("Unknown path fill rule");
  int winding{};
  for (const auto &[a, b] : segments) {
    const double cross = (double(b.x) - a.x) * (double(point.y) - a.y) -
                         (double(point.x) - a.x) * (double(b.y) - a.y);
    if (a.y <= point.y && b.y > point.y && cross > 0)
      ++winding;
    if (a.y > point.y && b.y <= point.y && cross < 0)
      --winding;
  }
  return rule == FillRule::EvenOdd ? winding % 2 != 0 : winding != 0;
}
bool FlattenedPath::touchesStroke(Point2 point, float radius) const {
  const double squared = double(radius) * radius;
  return std::ranges::any_of(segments, [&](const auto &s) {
    return distanceSquared(point, s.from, s.to) <= squared;
  });
}
} // namespace playground::math
