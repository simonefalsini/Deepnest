#include "deepnest/NestPartRepository.h"

#include "deepnest/ClipperBridge.h"
#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace deepnest {

namespace {

Coordinate PointSegmentDistanceSquared(const Point& point, const Point& start,
                                       const Point& end) {
  const Coordinate dx = end.x() - start.x();
  const Coordinate dy = end.y() - start.y();
  if (AlmostEqual(dx, 0.0) && AlmostEqual(dy, 0.0)) {
    return DistanceSquared(point, start);
  }

  const Coordinate t = ((point.x() - start.x()) * dx +
                        (point.y() - start.y()) * dy) /
                       (dx * dx + dy * dy);
  const Coordinate clamped = std::max<Coordinate>(0.0, std::min<Coordinate>(1.0, t));
  const Coordinate proj_x = start.x() + clamped * dx;
  const Coordinate proj_y = start.y() + clamped * dy;
  const Coordinate diff_x = point.x() - proj_x;
  const Coordinate diff_y = point.y() - proj_y;
  return diff_x * diff_x + diff_y * diff_y;
}

void DouglasPeucker(const std::vector<Point>& points, std::size_t first,
                    std::size_t last, Coordinate sq_tolerance,
                    std::vector<bool>* keep) {
  if (last <= first + 1) {
    return;
  }

  Coordinate max_distance = 0.0;
  std::optional<std::size_t> index;
  for (std::size_t i = first + 1; i < last; ++i) {
    const Coordinate distance = PointSegmentDistanceSquared(points[i], points[first],
                                                            points[last]);
    if (distance > max_distance) {
      max_distance = distance;
      index = i;
    }
  }

  if (!index.has_value() || max_distance <= sq_tolerance) {
    return;
  }

  (*keep)[index.value()] = true;
  DouglasPeucker(points, first, index.value(), sq_tolerance, keep);
  DouglasPeucker(points, index.value(), last, sq_tolerance, keep);
}

Loop SimplifyLoop(const Loop& loop, Coordinate tolerance) {
  if (loop.size() < 3 || tolerance <= 0.0) {
    return loop;
  }

  std::vector<Point> points(loop.begin(), loop.end());
  if (points.empty()) {
    return loop;
  }
  if (!AlmostEqualPoints(points.front(), points.back(), tolerance)) {
    points.push_back(points.front());
  }

  const Coordinate sq_tolerance = tolerance * tolerance;
  std::vector<bool> keep(points.size(), false);
  keep.front() = true;
  keep.back() = true;

  DouglasPeucker(points, 0, points.size() - 1, sq_tolerance, &keep);

  Loop simplified;
  simplified.reserve(points.size());
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    if (keep[i]) {
      simplified.emplace_back(points[i]);
    }
  }

  if (simplified.size() >= 3 && AlmostEqualPoints(simplified.front(),
                                                  simplified.back())) {
    simplified.pop_back();
  }

  if (simplified.size() < 3) {
    return loop;
  }

  return simplified;
}

Loop ApplyEndpointTolerance(const Loop& loop, Coordinate tolerance) {
  if (loop.size() < 2 || tolerance <= 0.0) {
    return loop;
  }

  Loop filtered;
  filtered.reserve(loop.size());
  for (const auto& point : loop) {
    if (filtered.empty() ||
        !WithinDistance(filtered.back(), point, tolerance)) {
      filtered.push_back(point);
    }
  }

  if (filtered.size() >= 2 &&
      WithinDistance(filtered.front(), filtered.back(), tolerance)) {
    filtered.pop_back();
  }

  if (filtered.size() < 3) {
    return loop;
  }

  return filtered;
}

PolygonWithHoles BuildPolygonWithHoles(const Loop& outer,
                                       const std::vector<Loop>& holes) {
  PolygonWithHoles polygon;
  Polygon outer_polygon;
  boost::polygon::set_points(outer_polygon, outer.begin(), outer.end());
  boost::polygon::set_outer(polygon, outer_polygon);
  for (const auto& hole : holes) {
    if (hole.size() < 3) {
      continue;
    }
    Polygon hole_polygon;
    boost::polygon::set_points(hole_polygon, hole.begin(), hole.end());
    boost::polygon::add_hole(polygon, hole_polygon);
  }
  return polygon;
}

PolygonWithHoles PolygonFromHoleLoop(const Loop& loop) {
  Loop normalized = loop;
  if (OrientationOfLoop(normalized) != Orientation::kCounterClockwise) {
    std::reverse(normalized.begin(), normalized.end());
  }
  return BuildPolygonWithHoles(normalized, {});
}

PolygonWithHoles SelectLargestPolygon(const PolygonCollection& polygons) {
  if (polygons.empty()) {
    return PolygonWithHoles();
  }

  return *std::max_element(
      polygons.begin(), polygons.end(),
      [](const PolygonWithHoles& lhs, const PolygonWithHoles& rhs) {
        return std::abs(ComputeArea(lhs)) < std::abs(ComputeArea(rhs));
      });
}

}  // namespace

NestPartRepository::NestPartRepository(const DeepNestConfig& config)
    : config_(config) {}

void NestPartRepository::LoadSheet(const QPainterPath& sheet) {
  sheet_ = PreparePolygon(QStringLiteral("sheet"), sheet, /*is_sheet=*/true);
  const BoundingBox bounds = ComputeBoundingBox(sheet_.geometry());
  if (!bounds.IsEmpty()) {
    sheet_.Translate(-bounds.min_x, -bounds.min_y);
  }
  sheet_loaded_ = sheet_.geometry().outer().size() >= 3;
}

void NestPartRepository::LoadParts(const QHash<QString, QPainterPath>& parts) {
  parts_.clear();
  parts_.reserve(parts.size());
  for (auto it = parts.constBegin(); it != parts.constEnd(); ++it) {
    parts_.push_back(PreparePolygon(it.key(), it.value(), /*is_sheet=*/false));
  }
}

NestPolygon NestPartRepository::PreparePolygon(const QString& id,
                                               const QPainterPath& path,
                                               bool is_sheet) {
  PolygonWithHoles polygon = QPainterPathToPolygonWithHoles(path);

  if (config_.simplify()) {
    polygon = SimplifyPolygon(polygon, /*inside=*/is_sheet);
  }

  const Coordinate spacing_delta =
      (config_.spacing() * 0.5) * (is_sheet ? -1.0 : 1.0);
  polygon = ApplySpacing(polygon, spacing_delta);

  if (!config_.use_holes()) {
    PolygonWithHoles without_holes;
    boost::polygon::set_outer(without_holes, polygon.outer());
    polygon = without_holes;
  }

  NestPolygon result(polygon);
  result.set_id(id);
  result.set_exact(!config_.simplify());
  PopulateChildren(&result);
  return result;
}

PolygonWithHoles NestPartRepository::SimplifyPolygon(
    const PolygonWithHoles& polygon, bool inside) const {
  const Coordinate tolerance =
      inside ? config_.endpoint_tolerance() : config_.curve_tolerance();
  Loop outer_loop;
  outer_loop.reserve(polygon.outer().size());
  for (const auto& point : polygon.outer()) {
    outer_loop.emplace_back(point);
  }

  outer_loop = ApplyEndpointTolerance(outer_loop, config_.endpoint_tolerance());
  outer_loop = SimplifyLoop(outer_loop, tolerance);
  if (OrientationOfLoop(outer_loop) != Orientation::kCounterClockwise) {
    std::reverse(outer_loop.begin(), outer_loop.end());
  }

  std::vector<Loop> inner_loops;
  inner_loops.reserve(polygon.inners().size());
  for (const auto& hole : polygon.inners()) {
    Loop loop;
    loop.reserve(hole.size());
    for (const auto& point : hole) {
      loop.emplace_back(point);
    }
    loop = ApplyEndpointTolerance(loop, config_.endpoint_tolerance());
    loop = SimplifyLoop(loop, tolerance);
    if (OrientationOfLoop(loop) != Orientation::kClockwise) {
      std::reverse(loop.begin(), loop.end());
    }
    inner_loops.emplace_back(std::move(loop));
  }

  return BuildPolygonWithHoles(outer_loop, inner_loops);
}

PolygonWithHoles NestPartRepository::ApplySpacing(
    const PolygonWithHoles& polygon, Coordinate delta) const {
  if (std::abs(delta) <= kTolerance) {
    return polygon;
  }

  PolygonCollection offset = OffsetPolygon(
      polygon, delta, clipper2::JoinType::Miter, clipper2::EndType::Polygon,
      /*miter_limit=*/4.0, config_.curve_tolerance());

  offset = CleanPolygons(offset, config_.endpoint_tolerance());
  if (offset.empty()) {
    return polygon;
  }

  return SelectLargestPolygon(offset);
}

void NestPartRepository::PopulateChildren(NestPolygon* polygon) const {
  if (!polygon) {
    return;
  }

  polygon->ClearChildren();
  if (!config_.use_holes()) {
    return;
  }

  const auto& geometry = polygon->geometry();
  std::size_t index = 0;
  for (const auto& hole : geometry.inners()) {
    Loop loop;
    loop.reserve(hole.size());
    for (const auto& point : hole) {
      loop.emplace_back(point);
    }
    PolygonWithHoles child_poly = PolygonFromHoleLoop(loop);
    NestPolygon child(child_poly);
    child.set_id(QStringLiteral("%1/hole%2").arg(polygon->id()).arg(index));
    child.set_exact(polygon->exact());
    polygon->AddChild(child);
    ++index;
  }
}

}  // namespace deepnest
