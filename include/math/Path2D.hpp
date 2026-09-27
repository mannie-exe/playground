#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <math/Geometry2D.hpp>

namespace playground::math {

enum class PathVerb { Move, Line, Quadratic, Cubic, Close };
enum class FillRule { NonZero, EvenOdd };

struct PathCommand {
  PathVerb verb;
  Point2 end;
  Point2 control1;
  Point2 control2;
  bool operator==(const PathCommand &) const = default;
};

class Path2D {
  std::vector<PathCommand> _commands;

public:
  Path2D &moveTo(Point2 point);
  Path2D &lineTo(Point2 point);
  Path2D &quadraticTo(Point2 control, Point2 end);
  Path2D &cubicTo(Point2 first, Point2 second, Point2 end);
  Path2D &close();

  std::span<const PathCommand> commands() const noexcept { return _commands; }

  bool operator==(const Path2D &) const = default;
};

struct PathSegment {
  Point2 from, to;
};

struct FlattenedPath {
  std::vector<PathSegment> segments;
  Rect bounds;

  bool contains(Point2 point, FillRule rule) const;
  bool touchesStroke(Point2 point, float radius) const;
};

struct PathFlattenProps {
  float tolerance{0.25f};
  std::size_t maximumSegments{4096};
  unsigned maximumDepth{16};
  bool closeOpenContours{true};
};

// Tolerance is in local units. Renderers account for the current pixel scale.
FlattenedPath flattenPath(const Path2D &, PathFlattenProps = {});

} // namespace playground::math
