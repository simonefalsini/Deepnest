#include "deepnest/ClipperBridge.h"

namespace deepnest {

clipper2::PathsD PolygonToClipper(const PolygonWithHoles& polygon) {
  clipper2::PathsD paths;
  clipper2::PathD outer;
  for (const auto& pt : polygon.outer()) {
    outer.emplace_back(pt.x(), pt.y());
  }
  paths.push_back(outer);
  for (const auto& hole : polygon.inners()) {
    clipper2::PathD hole_path;
    for (const auto& pt : hole) {
      hole_path.emplace_back(pt.x(), pt.y());
    }
    paths.push_back(hole_path);
  }
  return paths;
}

PolygonWithHoles ClipperToPolygon(const clipper2::PathsD& paths) {
  Polygon outer;
  if (paths.empty()) {
    return PolygonWithHoles();
  }
  const auto& first = paths.front();
  std::vector<Point> points;
  for (const auto& pt : first) {
    points.emplace_back(pt.x, pt.y);
  }
  boost::polygon::set_points(outer, points.begin(), points.end());
  PolygonWithHoles poly;
  boost::polygon::set_outer(poly, outer);
  return poly;
}

PolygonWithHoles OffsetPolygon(const PolygonWithHoles& polygon, Coordinate delta) {
  auto paths = PolygonToClipper(polygon);
  clipper2::ClipperOffset offset;
  clipper2::PathsD result;
  offset.AddPaths(paths, clipper2::JoinType::Round, clipper2::EndType::Polygon);
  offset.Execute(delta, result);
  return ClipperToPolygon(result);
}

}  // namespace deepnest
