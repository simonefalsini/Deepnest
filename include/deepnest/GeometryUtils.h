#pragma once

#include "deepnest/GeometryTypes.h"

#include <optional>
#include <utility>

namespace deepnest {

bool AlmostEqualPoints(const Point& lhs, const Point& rhs,
                       Coordinate tolerance = kTolerance);

Coordinate DistanceSquared(const Point& lhs, const Point& rhs);

Coordinate Distance(const Point& lhs, const Point& rhs);

bool WithinDistance(const Point& lhs, const Point& rhs, Coordinate distance);

Point NormalizeVector(const Point& vector);

Point MakeVector(const Point& from, const Point& to);

Coordinate Dot(const Point& lhs, const Point& rhs);

Coordinate Cross(const Point& lhs, const Point& rhs);

Coordinate Cross(const Point& origin, const Point& a, const Point& b);

Orientation OrientationOf(const Point& a, const Point& b, const Point& c,
                          Coordinate tolerance = kTolerance);

bool IsPointOnSegment(const Point& a, const Point& b, const Point& p,
                      Coordinate tolerance = kTolerance);

std::optional<Point> SegmentIntersection(const Point& a, const Point& b,
                                         const Point& c, const Point& d,
                                         bool treat_as_infinite = false,
                                         Coordinate tolerance = kTolerance);

Loop FindIntersections(const Loop& a, const Loop& b,
                       Coordinate tolerance = kTolerance);

BoundingBox ComputeBoundingBox(const Loop& loop);

BoundingBox ComputeBoundingBox(const Polygon& polygon);

BoundingBox ComputeBoundingBox(const PolygonWithHoles& polygon);

BoundingBox ComputeBoundingBox(const LoopCollection& loops);

Coordinate ComputeArea(const Loop& loop);

Coordinate ComputeArea(const Polygon& polygon);

Coordinate ComputeArea(const PolygonWithHoles& polygon);

Point ComputeCentroid(const Loop& loop);

Point ComputeCentroid(const Polygon& polygon);

Point ComputeCentroid(const PolygonWithHoles& polygon);

std::optional<bool> PointInLoop(const Loop& loop, const Point& point,
                                Coordinate tolerance = kTolerance);

std::optional<bool> PointInPolygon(const PolygonWithHoles& polygon,
                                   const Point& point,
                                   Coordinate tolerance = kTolerance);

Loop ComputeConvexHull(const Loop& loop,
                       Coordinate tolerance = kTolerance);

LoopCollection ComputeConvexHulls(const LoopCollection& loops,
                                  Coordinate tolerance = kTolerance);

}  // namespace deepnest
