#include "deepnest/ClipperBridge.h"

#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>

namespace deepnest {

namespace {

constexpr int kClipperPrecision = 6;

bool PointsEqual(const clipper2::PointD& lhs, const clipper2::PointD& rhs,
                 Coordinate tolerance = kTolerance) {
  return std::abs(lhs.x - rhs.x) <= tolerance &&
         std::abs(lhs.y - rhs.y) <= tolerance;
}

bool PointsEqual(const Point& lhs, const Point& rhs,
                 Coordinate tolerance = kTolerance) {
  return AlmostEqual(lhs.x(), rhs.x(), tolerance) &&
         AlmostEqual(lhs.y(), rhs.y(), tolerance);
}

Loop PolygonOuterToLoop(const Polygon& polygon) {
  Loop loop;
  loop.reserve(polygon.size());
  for (const auto& point : polygon) {
    loop.emplace_back(point.x(), point.y());
  }
  if (loop.size() > 1 && PointsEqual(loop.front(), loop.back())) {
    loop.pop_back();
  }
  return loop;
}

std::vector<Loop> PolygonInnersToLoops(const PolygonWithHoles& polygon) {
  std::vector<Loop> holes;
  holes.reserve(Holes(polygon).size());
  for (const auto& inner : Holes(polygon)) {
    Loop loop;
    loop.reserve(inner.size());
    for (const auto& point : inner) {
      loop.emplace_back(point.x(), point.y());
    }
    if (loop.size() > 1 && PointsEqual(loop.front(), loop.back())) {
      loop.pop_back();
    }
    holes.emplace_back(std::move(loop));
  }
  return holes;
}

Polygon PolygonFromLoop(const Loop& loop) {
  Polygon polygon;
  boost::polygon::set_points(polygon, loop.begin(), loop.end());
  return polygon;
}

PolygonWithHoles PolygonWithHolesFromLoops(const Loop& outer,
                                           const std::vector<Loop>& holes) {
  PolygonWithHoles polygon;
  SetOuter(polygon, PolygonFromLoop(outer));
  for (const auto& hole_loop : holes) {
    if (hole_loop.size() < 3) {
      continue;
    }
    AddHole(polygon, PolygonFromLoop(hole_loop));
  }
  return polygon;
}

clipper2::PathD LoopToClipperPath(const Loop& loop) {
  clipper2::PathD path;
  path.reserve(loop.size());
  for (const auto& point : loop) {
    path.emplace_back(point.x(), point.y());
  }
  if (path.size() > 1 && PointsEqual(path.front(), path.back())) {
    path.pop_back();
  }
  return path;
}

Loop ClipperPathToLoop(const clipper2::PathD& path) {
  Loop loop;
  loop.reserve(path.size());
  for (const auto& point : path) {
    loop.emplace_back(point.x, point.y);
  }
  if (loop.size() > 1 && PointsEqual(loop.front(), loop.back())) {
    loop.pop_back();
  }
  return loop;
}

clipper2::PathsD RemoveDegenerate(const clipper2::PathsD& paths) {
  clipper2::PathsD filtered;
  filtered.reserve(paths.size());
  for (const auto& path : paths) {
    if (path.size() < 3 || std::abs(clipper2::Area(path)) <= kTolerance) {
      continue;
    }
    filtered.push_back(path);
  }
  return filtered;
}

struct OuterRecord {
  Loop loop;
  Polygon polygon;
  PolygonWithHoles aggregate;
};

void EnsureLoopOrientation(Loop* loop, Orientation desired) {
  if (!loop || loop->size() < 3) {
    return;
  }
  if (OrientationOfLoop(*loop) != desired) {
    std::reverse(loop->begin(), loop->end());
  }
}

std::optional<Point> RepresentativePoint(const Loop& loop) {
  if (loop.empty()) {
    return std::nullopt;
  }
  return loop.front();
}

}  // namespace

clipper2::PathD LoopToClipper(const Loop& loop) {
  return LoopToClipperPath(loop);
}

Loop ClipperToLoop(const clipper2::PathD& path) {
  return ClipperPathToLoop(path);
}

clipper2::PathsD PolygonToClipper(const PolygonWithHoles& polygon) {
  clipper2::PathsD paths;
  const Loop outer_loop = PolygonOuterToLoop(Outer(polygon));
  if (outer_loop.size() < 3) {
    return paths;
  }

  Loop normalized_outer = outer_loop;
  EnsureLoopOrientation(&normalized_outer, Orientation::kCounterClockwise);
  paths.emplace_back(LoopToClipperPath(normalized_outer));

  auto holes = PolygonInnersToLoops(polygon);
  for (auto& hole : holes) {
    if (hole.size() < 3) {
      continue;
    }
    EnsureLoopOrientation(&hole, Orientation::kClockwise);
    paths.emplace_back(LoopToClipperPath(hole));
  }
  return paths;
}

clipper2::PathsD PolygonCollectionToClipper(const PolygonCollection& polygons) {
  clipper2::PathsD paths;
  for (const auto& polygon : polygons) {
    const auto converted = PolygonToClipper(polygon);
    paths.insert(paths.end(), converted.begin(), converted.end());
  }
  return paths;
}

PolygonWithHoles ClipperToPolygon(const clipper2::PathsD& paths) {
  const auto collection = ClipperToPolygonCollection(paths);
  if (collection.empty()) {
    return PolygonWithHoles();
  }
  return collection.front();
}

PolygonCollection ClipperToPolygonCollection(const clipper2::PathsD& paths) {
  PolygonCollection collection;
  const auto filtered = RemoveDegenerate(paths);
  if (filtered.empty()) {
    return collection;
  }

  std::vector<OuterRecord> outers;
  std::vector<Loop> holes;
  outers.reserve(filtered.size());

  for (const auto& path : filtered) {
    Loop loop = ClipperPathToLoop(path);
    if (loop.size() < 3) {
      continue;
    }

    const double area = clipper2::Area(path);
    if (area >= 0.0) {
      EnsureLoopOrientation(&loop, Orientation::kCounterClockwise);
      OuterRecord record;
      record.loop = loop;
      record.polygon = PolygonFromLoop(loop);
      record.aggregate = PolygonWithHolesFromLoops(loop, {});
      outers.emplace_back(std::move(record));
    } else {
      EnsureLoopOrientation(&loop, Orientation::kClockwise);
      holes.emplace_back(std::move(loop));
    }
  }

  for (const auto& hole : holes) {
    const auto repr = RepresentativePoint(hole);
    if (!repr.has_value()) {
      continue;
    }

    bool assigned = false;
    for (auto& outer : outers) {
      if (boost::polygon::contains(outer.polygon, repr.value())) {
        AddHole(outer.aggregate, PolygonFromLoop(hole));
        assigned = true;
        break;
      }
    }

    if (!assigned) {
      // Treat unassigned holes as independent polygons to avoid data loss.
      Loop promoted = hole;
      EnsureLoopOrientation(&promoted, Orientation::kCounterClockwise);
      OuterRecord record;
      record.loop = promoted;
      record.polygon = PolygonFromLoop(promoted);
      record.aggregate = PolygonWithHolesFromLoops(promoted, {});
      outers.emplace_back(std::move(record));
    }
  }

  collection.reserve(outers.size());
  for (auto& outer : outers) {
    collection.emplace_back(std::move(outer.aggregate));
  }

  return collection;
}

clipper2::Paths64 ScaleUpPaths(const clipper2::PathsD& paths, double scale) {
  clipper2::Paths64 scaled;
  scaled.reserve(paths.size());
  for (const auto& path : paths) {
    clipper2::Path64 scaled_path;
    scaled_path.reserve(path.size());
    for (const auto& point : path) {
      scaled_path.emplace_back(static_cast<int64_t>(std::llround(point.x * scale)),
                               static_cast<int64_t>(std::llround(point.y * scale)));
    }
    scaled.push_back(std::move(scaled_path));
  }
  return scaled;
}

clipper2::PathsD ScaleDownPaths(const clipper2::Paths64& paths, double scale) {
  clipper2::PathsD scaled;
  scaled.reserve(paths.size());
  if (scale == 0.0) {
    return scaled;
  }
  const double inv_scale = 1.0 / scale;
  for (const auto& path : paths) {
    clipper2::PathD scaled_path;
    scaled_path.reserve(path.size());
    for (const auto& point : path) {
      scaled_path.emplace_back(static_cast<double>(point.x) * inv_scale,
                               static_cast<double>(point.y) * inv_scale);
    }
    scaled.push_back(std::move(scaled_path));
  }
  return scaled;
}

clipper2::PathsD NormalizeClipperOrientation(const clipper2::PathsD& paths,
                                             bool outer_ccw) {
  if (paths.empty()) {
    return paths;
  }

  auto polygons = ClipperToPolygonCollection(paths);
  auto normalized = PolygonCollectionToClipper(polygons);

  if (!outer_ccw) {
    for (auto& path : normalized) {
      std::reverse(path.begin(), path.end());
    }
  }
  return normalized;
}

clipper2::PathsD CleanPaths(const clipper2::PathsD& paths, Coordinate tolerance) {
  if (paths.empty()) {
    return paths;
  }

  clipper2::PathsD cleaned;
  cleaned.reserve(paths.size());

  for (const auto& path : paths) {
    if (path.size() < 3) {
      continue;
    }
    clipper2::PathD simplified =
        tolerance > 0.0 ? clipper2::SimplifyPath(path, tolerance, true)
                        : path;

    if (simplified.size() < 3) {
      continue;
    }

    if (PointsEqual(simplified.front(), simplified.back(), tolerance)) {
      simplified.pop_back();
    }
    if (simplified.size() >= 3) {
      cleaned.emplace_back(std::move(simplified));
    }
  }

  return NormalizeClipperOrientation(cleaned);
}

PolygonCollection CleanPolygons(const PolygonCollection& polygons,
                                Coordinate tolerance) {
  if (polygons.empty()) {
    return polygons;
  }

  const auto as_paths = PolygonCollectionToClipper(polygons);
  const auto cleaned_paths = CleanPaths(as_paths, tolerance);
  return ClipperToPolygonCollection(cleaned_paths);
}

PolygonCollection OffsetPolygon(const PolygonWithHoles& polygon,
                                Coordinate delta,
                                clipper2::JoinType join_type,
                                clipper2::EndType end_type,
                                double miter_limit,
                                Coordinate arc_tolerance) {
  if (std::abs(delta) <= kTolerance) {
    return {polygon};
  }

  const auto paths = PolygonToClipper(polygon);
  if (paths.empty()) {
    return {};
  }

  const auto scaled = ScaleUpPaths(paths, kClipperScale);
  clipper2::ClipperOffset offset(miter_limit, arc_tolerance * kClipperScale);
  offset.AddPaths(scaled, join_type, end_type);

  clipper2::Paths64 solution;
  offset.Execute(delta * kClipperScale, solution);

  const auto downscaled = ScaleDownPaths(solution, kClipperScale);
  const auto normalized = NormalizeClipperOrientation(downscaled);
  return ClipperToPolygonCollection(normalized);
}

PolygonCollection BooleanDifference(const PolygonCollection& subject,
                                    const PolygonCollection& clips,
                                    clipper2::FillRule fill_rule) {
  if (subject.empty()) {
    return {};
  }

  const auto subject_paths = NormalizeClipperOrientation(
      PolygonCollectionToClipper(subject));
  const auto clip_paths = NormalizeClipperOrientation(
      PolygonCollectionToClipper(clips));

  const auto difference = clipper2::Difference(subject_paths, clip_paths,
                                               fill_rule, kClipperPrecision);
  return ClipperToPolygonCollection(difference);
}

PolygonCollection BooleanDifference(const PolygonWithHoles& subject,
                                    const PolygonCollection& clips,
                                    clipper2::FillRule fill_rule) {
  PolygonCollection subjects;
  subjects.push_back(subject);
  return BooleanDifference(subjects, clips, fill_rule);
}

}  // namespace deepnest
