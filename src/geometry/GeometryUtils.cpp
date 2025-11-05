#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>

#include <boost/polygon/polygon_traits.hpp>

namespace deepnest {
namespace {

Loop LoopFromPolygon(const Polygon& polygon) {
  Loop loop;
  loop.reserve(polygon.size());
  for (const auto& point : polygon) {
    loop.emplace_back(point);
  }
  return loop;
}

std::pair<Coordinate, Point> AccumulateCentroid(const Loop& loop) {
  const std::size_t size = loop.size();
  if (size < 3) {
    return {0.0, Point(0, 0)};
  }

  Coordinate signed_area = 0.0;
  Coordinate centroid_x = 0.0;
  Coordinate centroid_y = 0.0;

  for (std::size_t i = 0; i < size; ++i) {
    const auto& current = loop[i];
    const auto& next = loop[(i + 1) % size];
    const Coordinate cross = current.x() * next.y() - next.x() * current.y();
    signed_area += cross;
    centroid_x += (current.x() + next.x()) * cross;
    centroid_y += (current.y() + next.y()) * cross;
  }

  signed_area /= 2.0;
  if (AlmostEqual(signed_area, 0.0)) {
    return {0.0, Point(0, 0)};
  }

  const Coordinate factor = 1.0 / (6.0 * signed_area);
  return {signed_area, Point(centroid_x * factor, centroid_y * factor)};
}

bool IsWithinBounds(Coordinate value, Coordinate min_value, Coordinate max_value,
                    Coordinate tolerance) {
  if (min_value > max_value) {
    std::swap(min_value, max_value);
  }
  return value + tolerance >= min_value && value - tolerance <= max_value;
}

}  // namespace

bool AlmostEqualPoints(const Point& lhs, const Point& rhs, Coordinate tolerance) {
  return DistanceSquared(lhs, rhs) <= tolerance * tolerance;
}

Coordinate DistanceSquared(const Point& lhs, const Point& rhs) {
  const Coordinate dx = lhs.x() - rhs.x();
  const Coordinate dy = lhs.y() - rhs.y();
  return (dx * dx) + (dy * dy);
}

Coordinate Distance(const Point& lhs, const Point& rhs) {
  return std::sqrt(DistanceSquared(lhs, rhs));
}

bool WithinDistance(const Point& lhs, const Point& rhs, Coordinate distance) {
  return DistanceSquared(lhs, rhs) < (distance * distance);
}

Point NormalizeVector(const Point& vector) {
  const Coordinate length_squared = DistanceSquared(vector, Point(0, 0));
  if (AlmostEqual(length_squared, 0.0)) {
    return Point(0, 0);
  }

  const Coordinate length = std::sqrt(length_squared);
  return Point(vector.x() / length, vector.y() / length);
}

Point MakeVector(const Point& from, const Point& to) {
  return Point(to.x() - from.x(), to.y() - from.y());
}

Coordinate Dot(const Point& lhs, const Point& rhs) {
  return lhs.x() * rhs.x() + lhs.y() * rhs.y();
}

Coordinate Cross(const Point& lhs, const Point& rhs) {
  return lhs.x() * rhs.y() - lhs.y() * rhs.x();
}

Coordinate Cross(const Point& origin, const Point& a, const Point& b) {
  return Cross(MakeVector(origin, a), MakeVector(origin, b));
}

Orientation OrientationOf(const Point& a, const Point& b, const Point& c,
                          Coordinate tolerance) {
  const Coordinate cross_value = Cross(a, b, c);
  if (cross_value > tolerance) {
    return Orientation::kCounterClockwise;
  }
  if (cross_value < -tolerance) {
    return Orientation::kClockwise;
  }
  return Orientation::kCollinear;
}

bool IsPointOnSegment(const Point& a, const Point& b, const Point& p,
                      Coordinate tolerance) {
  if (AlmostEqualPoints(a, p, tolerance) || AlmostEqualPoints(b, p, tolerance)) {
    return false;
  }

  if (!IsWithinBounds(p.x(), a.x(), b.x(), tolerance) ||
      !IsWithinBounds(p.y(), a.y(), b.y(), tolerance)) {
    return false;
  }

  const Coordinate cross = Cross(a, b, p);
  if (!AlmostEqual(cross, 0.0, tolerance)) {
    return false;
  }

  const Point ab = MakeVector(a, b);
  const Point ap = MakeVector(a, p);
  const Coordinate dot = Dot(ap, ab);
  if (dot < -tolerance) {
    return false;
  }
  const Coordinate length_squared = Dot(ab, ab);
  if (dot - length_squared > tolerance) {
    return false;
  }
  return true;
}

std::optional<Point> SegmentIntersection(const Point& a, const Point& b,
                                         const Point& c, const Point& d,
                                         bool treat_as_infinite,
                                         Coordinate tolerance) {
  const Coordinate a1 = b.y() - a.y();
  const Coordinate b1 = a.x() - b.x();
  const Coordinate c1 = b.x() * a.y() - a.x() * b.y();

  const Coordinate a2 = d.y() - c.y();
  const Coordinate b2 = c.x() - d.x();
  const Coordinate c2 = d.x() * c.y() - c.x() * d.y();

  const Coordinate denom = a1 * b2 - a2 * b1;
  if (AlmostEqual(denom, 0.0, tolerance)) {
    return std::nullopt;
  }

  const Coordinate x = (b1 * c2 - b2 * c1) / denom;
  const Coordinate y = (a2 * c1 - a1 * c2) / denom;

  if (!std::isfinite(x) || !std::isfinite(y)) {
    return std::nullopt;
  }

  if (!treat_as_infinite) {
    if (!IsWithinBounds(x, a.x(), b.x(), tolerance) ||
        !IsWithinBounds(y, a.y(), b.y(), tolerance)) {
      return std::nullopt;
    }
    if (!IsWithinBounds(x, c.x(), d.x(), tolerance) ||
        !IsWithinBounds(y, c.y(), d.y(), tolerance)) {
      return std::nullopt;
    }
  }

  return Point(x, y);
}

Loop FindIntersections(const Loop& a, const Loop& b,
                       Coordinate tolerance) {
  Loop intersections;
  if (a.size() < 2 || b.size() < 2) {
    return intersections;
  }

  for (std::size_t i = 0; i < a.size(); ++i) {
    const auto& a_start = a[i];
    const auto& a_end = a[(i + 1) % a.size()];
    if (AlmostEqualPoints(a_start, a_end, tolerance)) {
      continue;
    }

    for (std::size_t j = 0; j < b.size(); ++j) {
      const auto& b_start = b[j];
      const auto& b_end = b[(j + 1) % b.size()];
      if (AlmostEqualPoints(b_start, b_end, tolerance)) {
        continue;
      }

      const auto intersection =
          SegmentIntersection(a_start, a_end, b_start, b_end, false, tolerance);
      if (!intersection.has_value()) {
        continue;
      }

      const bool duplicate = std::any_of(
          intersections.begin(), intersections.end(),
          [&](const Point& existing) {
            return AlmostEqualPoints(existing, intersection.value(), tolerance);
          });
      if (!duplicate) {
        intersections.push_back(intersection.value());
      }
    }
  }

  return intersections;
}

BoundingBox ComputeBoundingBox(const Loop& loop) {
  BoundingBox box = BoundingBox::Empty();
  for (const auto& point : loop) {
    box.Expand(point);
  }
  return box;
}

BoundingBox ComputeBoundingBox(const Polygon& polygon) {
  BoundingBox box = BoundingBox::Empty();
  for (const auto& point : polygon) {
    box.Expand(point);
  }
  return box;
}

BoundingBox ComputeBoundingBox(const PolygonWithHoles& polygon) {
  BoundingBox box = ComputeBoundingBox(polygon.outer());
  for (const auto& hole : polygon.inners()) {
    box.Expand(ComputeBoundingBox(hole));
  }
  return box;
}

BoundingBox ComputeBoundingBox(const LoopCollection& loops) {
  BoundingBox box = BoundingBox::Empty();
  for (const auto& loop : loops) {
    box.Expand(ComputeBoundingBox(loop));
  }
  return box;
}

Coordinate ComputeArea(const Loop& loop) {
  return SignedArea(loop);
}

Coordinate ComputeArea(const Polygon& polygon) {
  return boost::polygon::area(polygon);
}

Coordinate ComputeArea(const PolygonWithHoles& polygon) {
  return boost::polygon::area(polygon);
}

Point ComputeCentroid(const Loop& loop) {
  auto [area, centroid] = AccumulateCentroid(loop);
  if (AlmostEqual(area, 0.0)) {
    return Point(0, 0);
  }
  return centroid;
}

Point ComputeCentroid(const Polygon& polygon) {
  Point centroid;
  boost::polygon::centroid(polygon, centroid);
  return centroid;
}

Point ComputeCentroid(const PolygonWithHoles& polygon) {
  const Loop outer_loop = LoopFromPolygon(polygon.outer());
  auto [outer_area, outer_centroid] = AccumulateCentroid(outer_loop);

  Coordinate total_area = outer_area;
  Coordinate centroid_x = outer_centroid.x() * outer_area;
  Coordinate centroid_y = outer_centroid.y() * outer_area;

  for (const auto& hole : polygon.inners()) {
    const Loop hole_loop = LoopFromPolygon(hole);
    auto [hole_area, hole_centroid] = AccumulateCentroid(hole_loop);
    total_area += hole_area;
    centroid_x += hole_centroid.x() * hole_area;
    centroid_y += hole_centroid.y() * hole_area;
  }

  if (AlmostEqual(total_area, 0.0)) {
    return Point(0, 0);
  }

  const Coordinate factor = 1.0 / total_area;
  return Point(centroid_x * factor, centroid_y * factor);
}

std::optional<bool> PointInLoop(const Loop& loop, const Point& point,
                                Coordinate tolerance) {
  if (loop.size() < 3) {
    return std::nullopt;
  }

  bool inside = false;
  for (std::size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++) {
    const auto& current = loop[i];
    const auto& prev = loop[j];

    if (AlmostEqualPoints(current, point, tolerance) ||
        AlmostEqualPoints(prev, point, tolerance)) {
      return std::nullopt;
    }

    if (IsPointOnSegment(prev, current, point, tolerance)) {
      return std::nullopt;
    }

    if (AlmostEqual(prev.x(), current.x(), tolerance) &&
        AlmostEqual(prev.y(), current.y(), tolerance)) {
      continue;
    }

    const bool intersects = ((prev.y() > point.y()) != (current.y() > point.y())) &&
                            (point.x() < (current.x() - prev.x()) *
                                                 (point.y() - prev.y()) /
                                                 (current.y() - prev.y()) +
                                             prev.x());
    if (intersects) {
      inside = !inside;
    }
  }

  return inside;
}

std::optional<bool> PointInPolygon(const PolygonWithHoles& polygon,
                                   const Point& point,
                                   Coordinate tolerance) {
  const auto outer_result =
      PointInLoop(LoopFromPolygon(polygon.outer()), point, tolerance);
  if (!outer_result.has_value()) {
    return std::nullopt;
  }
  if (!outer_result.value()) {
    return false;
  }

  for (const auto& hole : polygon.inners()) {
    const auto hole_result = PointInLoop(LoopFromPolygon(hole), point, tolerance);
    if (!hole_result.has_value()) {
      return std::nullopt;
    }
    if (hole_result.value()) {
      return false;
    }
  }

  return true;
}

Loop ComputeConvexHull(const Loop& loop, Coordinate tolerance) {
  if (loop.size() < 3) {
    return loop;
  }

  Loop points = loop;
  std::sort(points.begin(), points.end(), [tolerance](const Point& lhs, const Point& rhs) {
    if (!AlmostEqual(lhs.x(), rhs.x(), tolerance)) {
      return lhs.x() < rhs.x();
    }
    return lhs.y() < rhs.y();
  });

  auto unique_end = std::unique(points.begin(), points.end(),
                                [tolerance](const Point& lhs, const Point& rhs) {
                                  return AlmostEqualPoints(lhs, rhs, tolerance);
                                });
  points.erase(unique_end, points.end());

  if (points.size() < 3) {
    return points;
  }

  Loop lower;
  for (const auto& point : points) {
    while (lower.size() >= 2 &&
           Cross(lower[lower.size() - 2], lower.back(), point) <= tolerance) {
      lower.pop_back();
    }
    lower.push_back(point);
  }

  Loop upper;
  for (auto it = points.rbegin(); it != points.rend(); ++it) {
    while (upper.size() >= 2 &&
           Cross(upper[upper.size() - 2], upper.back(), *it) <= tolerance) {
      upper.pop_back();
    }
    upper.push_back(*it);
  }

  lower.pop_back();
  upper.pop_back();

  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}

LoopCollection ComputeConvexHulls(const LoopCollection& loops,
                                  Coordinate tolerance) {
  LoopCollection hulls;
  hulls.reserve(loops.size());
  for (const auto& loop : loops) {
    hulls.emplace_back(ComputeConvexHull(loop, tolerance));
  }
  return hulls;
}

}  // namespace deepnest
