#include "deepnest/NestPolygon.h"

#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace deepnest {

namespace {

Loop ExtractLoop(const Polygon& polygon) {
  Loop loop;
  loop.reserve(polygon.size());
  for (const auto& point : polygon) {
    loop.emplace_back(point.x(), point.y());
  }
  if (loop.size() > 1 && AlmostEqual(loop.front().x(), loop.back().x()) &&
      AlmostEqual(loop.front().y(), loop.back().y())) {
    loop.pop_back();
  }
  return loop;
}

std::vector<Loop> ExtractHoles(const PolygonWithHoles& polygon) {
  std::vector<Loop> holes;
  holes.reserve(Holes(polygon).size());
  for (const auto& inner : Holes(polygon)) {
    holes.emplace_back(ExtractLoop(inner));
  }
  return holes;
}

Polygon BuildPolygon(const Loop& loop) {
  Polygon polygon;
  boost::polygon::set_points(polygon, loop.begin(), loop.end());
  return polygon;
}

PolygonWithHoles BuildPolygonWithHoles(const Loop& outer,
                                       const std::vector<Loop>& holes) {
  PolygonWithHoles polygon;
  SetOuter(polygon, BuildPolygon(outer));
  for (const auto& hole : holes) {
    if (hole.size() < 3) {
      continue;
    }
    AddHole(polygon, BuildPolygon(hole));
  }
  return polygon;
}

void RotateLoop(Loop* loop, Coordinate radians) {
  if (!loop) {
    return;
  }
  const Coordinate cos_angle = std::cos(radians);
  const Coordinate sin_angle = std::sin(radians);
  for (auto& point : *loop) {
    const Coordinate x = point.x();
    const Coordinate y = point.y();
    point = Point(x * cos_angle - y * sin_angle,
                  x * sin_angle + y * cos_angle);
  }
}

void TranslateLoop(Loop* loop, Coordinate dx, Coordinate dy) {
  if (!loop) {
    return;
  }
  for (auto& point : *loop) {
    point = Point(point.x() + dx, point.y() + dy);
  }
}

}  // namespace

NestPolygon::NestPolygon()
    : rotation_(0.0),
      position_(0.0, 0.0),
      exact_(true) {}

NestPolygon::NestPolygon(const PolygonWithHoles& polygon)
    : geometry_(polygon),
      rotation_(0.0),
      position_(0.0, 0.0),
      exact_(true) {
  NormalizeOrientation();
  UpdateExactMetadata();
}

void NestPolygon::set_geometry(const PolygonWithHoles& polygon) {
  geometry_ = polygon;
  NormalizeOrientation();
  UpdateExactMetadata();
}

void NestPolygon::set_outer_vertex_exact(const std::vector<bool>& flags) {
  if (flags.size() == Outer(geometry_).size()) {
    outer_exact_ = flags;
  }
}

void NestPolygon::set_inner_vertex_exact(std::size_t index,
                                         const std::vector<bool>& flags) {
  if (index >= Holes(geometry_).size()) {
    return;
  }
  if (index >= inner_exact_.size()) {
    inner_exact_.resize(index + 1);
  }
  if (flags.size() == Holes(geometry_)[index].size()) {
    inner_exact_[index] = flags;
  }
}

void NestPolygon::AddChild(const NestPolygon& child) {
  NestPolygon clone = child.Clone();
  clone.NormalizeOrientation();
  children_.push_back(std::move(clone));
}

void NestPolygon::ClearChildren() {
  children_.clear();
}

NestPolygon NestPolygon::Clone() const {
  NestPolygon copy;
  copy.id_ = id_;
  copy.source_ = source_;
  copy.geometry_ = geometry_;
  copy.rotation_ = rotation_;
  copy.position_ = position_;
  copy.exact_ = exact_;
  copy.outer_exact_ = outer_exact_;
  copy.inner_exact_ = inner_exact_;
  copy.children_ = children_;
  return copy;
}

void NestPolygon::NormalizeOrientation() {
  Loop outer = ExtractLoop(Outer(geometry_));
  if (!outer.empty() &&
      OrientationOfLoop(outer) != Orientation::kCounterClockwise) {
    std::reverse(outer.begin(), outer.end());
  }

  auto holes = ExtractHoles(geometry_);
  for (auto& hole : holes) {
    if (hole.empty()) {
      continue;
    }
    if (OrientationOfLoop(hole) != Orientation::kClockwise) {
      std::reverse(hole.begin(), hole.end());
    }
  }

  geometry_ = BuildPolygonWithHoles(outer, holes);

  for (auto& child : children_) {
    child.NormalizeOrientation();
  }
}

void NestPolygon::Translate(Coordinate dx, Coordinate dy) {
  Loop outer = ExtractLoop(Outer(geometry_));
  TranslateLoop(&outer, dx, dy);

  auto holes = ExtractHoles(geometry_);
  for (auto& hole : holes) {
    TranslateLoop(&hole, dx, dy);
  }

  geometry_ = BuildPolygonWithHoles(outer, holes);
  position_ += QPointF(dx, dy);

  for (auto& child : children_) {
    child.Translate(dx, dy);
  }
}

void NestPolygon::Rotate(Coordinate degrees) {
  const Coordinate radians = DegreesToRadians(degrees);
  Loop outer = ExtractLoop(Outer(geometry_));
  RotateLoop(&outer, radians);

  auto holes = ExtractHoles(geometry_);
  for (auto& hole : holes) {
    RotateLoop(&hole, radians);
  }

  geometry_ = BuildPolygonWithHoles(outer, holes);
  rotation_ += degrees;
  while (rotation_ >= 360.0) {
    rotation_ -= 360.0;
  }
  while (rotation_ < 0.0) {
    rotation_ += 360.0;
  }

  for (auto& child : children_) {
    child.Rotate(degrees);
  }
}

void NestPolygon::UpdateExactMetadata() {
  const auto outer_size = Outer(geometry_).size();
  if (outer_size != outer_exact_.size()) {
    outer_exact_.assign(outer_size, exact_);
  }

  const auto inner_count = Holes(geometry_).size();
  inner_exact_.resize(inner_count);
  std::size_t index = 0;
  for (const auto& inner : Holes(geometry_)) {
    const auto required = inner.size();
    auto& flags = inner_exact_[index++];
    if (flags.size() != required) {
      flags.assign(required, exact_);
    }
  }
}

}  // namespace deepnest
