#include "deepnest/QtGeometryConverters.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QVector>

#include <algorithm>

namespace deepnest {

namespace {

bool PointsApproximatelyEqual(const Point& lhs, const Point& rhs,
                              Coordinate tolerance = kTolerance) {
  return AlmostEqual(lhs.x(), rhs.x(), tolerance) &&
         AlmostEqual(lhs.y(), rhs.y(), tolerance);
}

void RemoveTrailingDuplicate(Loop& loop) {
  if (loop.size() < 2) {
    return;
  }
  if (PointsApproximatelyEqual(loop.front(), loop.back())) {
    loop.pop_back();
  }
}

Loop ConvertSubpathToLoop(const QPolygonF& polygon, Coordinate scale) {
  Loop loop;
  loop.reserve(polygon.size());
  for (int i = 0; i < polygon.size(); ++i) {
    const auto& point = polygon.at(i);
    Point scaled(point.x() * scale, point.y() * scale);
    if (!loop.empty() && PointsApproximatelyEqual(loop.back(), scaled)) {
      continue;
    }
    loop.push_back(std::move(scaled));
  }
  RemoveTrailingDuplicate(loop);
  return loop;
}

LoopCollection ExtractLoops(const QPainterPath& path, Coordinate scale) {
  LoopCollection loops;
  const auto subpaths = path.toSubpathPolygons();
  loops.reserve(subpaths.size());
  for (const auto& polygon : subpaths) {
    Loop loop = ConvertSubpathToLoop(polygon, scale);
    if (loop.size() >= 3) {
      loops.emplace_back(std::move(loop));
    }
  }

  if (!loops.empty()) {
    return loops;
  }

  // Fallback: some painter paths expose only fill polygons.
  const auto fill_polygons = path.toFillPolygons();
  loops.clear();
  loops.reserve(fill_polygons.size());
  for (const auto& polygon : fill_polygons) {
    Loop loop = ConvertSubpathToLoop(polygon, scale);
    if (loop.size() >= 3) {
      loops.emplace_back(std::move(loop));
    }
  }
  return loops;
}

void EnsureOrientation(Loop& loop, Orientation desired) {
  if (loop.size() < 3 || desired == Orientation::kCollinear) {
    return;
  }
  const Orientation orientation = OrientationOfLoop(loop);
  if (orientation == Orientation::kCollinear) {
    return;
  }
  if (orientation != desired) {
    std::reverse(loop.begin(), loop.end());
  }
}

Polygon LoopToPolygon(const Loop& loop) {
  Polygon polygon;
  boost::polygon::set_points(polygon, loop.begin(), loop.end());
  return polygon;
}

Loop PolygonToLoop(const Polygon& polygon) {
  Loop loop;
  loop.reserve(polygon.size());
  for (const auto& point : polygon) {
    loop.emplace_back(point);
  }
  RemoveTrailingDuplicate(loop);
  return loop;
}

void AppendLoopToPainterPath(QPainterPath& path, const Loop& loop,
                             Coordinate scale) {
  if (loop.size() < 3) {
    return;
  }
  const auto& first = loop.front();
  path.moveTo(first.x() / scale, first.y() / scale);
  for (std::size_t i = 1; i < loop.size(); ++i) {
    const auto& point = loop[i];
    path.lineTo(point.x() / scale, point.y() / scale);
  }
  path.closeSubpath();
}

}  // namespace

Point QPointFToPoint(const QPointF& point, Coordinate scale) {
  return Point(point.x() * scale, point.y() * scale);
}

QPointF PointToQPointF(const Point& point, Coordinate scale) {
  return QPointF(point.x() / scale, point.y() / scale);
}

Loop QPolygonFToLoop(const QPolygonF& polygon, Coordinate scale) {
  return ConvertSubpathToLoop(polygon, scale);
}

QPolygonF LoopToQPolygonF(const Loop& loop, Coordinate scale) {
  QPolygonF polygon;
  polygon.reserve(static_cast<int>(loop.size()));
  for (const auto& point : loop) {
    polygon.append(PointToQPointF(point, scale));
  }
  if (!loop.empty()) {
    polygon.append(PointToQPointF(loop.front(), scale));
  }
  return polygon;
}

PolygonWithHoles QPainterPathToPolygonWithHoles(const QPainterPath& path,
                                               Coordinate scale) {
  PolygonWithHoles result;
  auto loops = ExtractLoops(path, scale);
  if (loops.empty()) {
    return result;
  }

  auto outer_iter = std::max_element(
      loops.begin(), loops.end(), [](const Loop& lhs, const Loop& rhs) {
        return std::abs(SignedArea(lhs)) < std::abs(SignedArea(rhs));
      });

  const std::size_t outer_index =
      static_cast<std::size_t>(std::distance(loops.begin(), outer_iter));
  Loop outer_loop = loops[outer_index];
  EnsureOrientation(outer_loop, Orientation::kCounterClockwise);
  SetOuter(result, LoopToPolygon(outer_loop));

  for (std::size_t i = 0; i < loops.size(); ++i) {
    if (i == outer_index) {
      continue;
    }
    Loop hole_loop = loops[i];
    EnsureOrientation(hole_loop, Orientation::kClockwise);
    Holes(result).push_back(LoopToPolygon(hole_loop));
  }
  return result;
}

QPainterPath PolygonWithHolesToQPainterPath(const PolygonWithHoles& polygon,
                                            Coordinate scale) {
  QPainterPath path;
  path.setFillRule(Qt::OddEvenFill);

  Loop outer_loop = PolygonToLoop(Outer(polygon));
  AppendLoopToPainterPath(path, outer_loop, scale);

  for (const auto& hole : Holes(polygon)) {
    Loop hole_loop = PolygonToLoop(hole);
    AppendLoopToPainterPath(path, hole_loop, scale);
  }

  return path;
}

}  // namespace deepnest
