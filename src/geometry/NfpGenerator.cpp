#include "deepnest/NfpGenerator.h"

#include "deepnest/GeometryUtils.h"

#include <algorithm>

namespace deepnest {

namespace {
PolygonWithHoles LoopToPolygon(const Loop& loop) {
  Loop normalized = loop;
  if (normalized.size() < 3) {
    return PolygonWithHoles();
  }
  if (OrientationOfLoop(normalized) != Orientation::kCounterClockwise) {
    std::reverse(normalized.begin(), normalized.end());
  }
  Polygon outer;
  boost::polygon::set_points(outer, normalized.begin(), normalized.end());
  PolygonWithHoles polygon;
  boost::polygon::set_outer(polygon, outer);
  return polygon;
}

PolygonCollection BuildHolePolygons(const NestPolygon& polygon) {
  PolygonCollection holes;
  for (const auto& child : polygon.children()) {
    holes.push_back(child.geometry());
  }
  return holes;
}

PolygonWithHoles BuildFramePolygon(const NestPolygon& polygon) {
  const BoundingBox bounds = ComputeBoundingBox(polygon.geometry());
  if (bounds.IsEmpty()) {
    return PolygonWithHoles();
  }

  const Coordinate width = bounds.Width();
  const Coordinate height = bounds.Height();
  const Coordinate expand_x = width * 0.05;
  const Coordinate expand_y = height * 0.05;

  const Coordinate x = bounds.min_x - expand_x;
  const Coordinate y = bounds.min_y - expand_y;
  const Coordinate w = width * 1.1;
  const Coordinate h = height * 1.1;

  Loop loop;
  loop.emplace_back(x, y);
  loop.emplace_back(x + w, y);
  loop.emplace_back(x + w, y + h);
  loop.emplace_back(x, y + h);
  return LoopToPolygon(loop);
}

PolygonCollection Difference(const PolygonCollection& subject,
                             const PolygonCollection& clips) {
  if (clips.empty()) {
    return subject;
  }
  return BooleanDifference(subject, clips);
}

}  // namespace

NfpGenerator::NfpGenerator(NfpCache* cache, const DeepNestConfig& config)
    : cache_(cache), config_(config) {}

PolygonCollection NfpGenerator::Compute(const NestPolygon& a,
                                        const NestPolygon& b, bool inside) {
  if (cache_) {
    cache_->SyncConfig(config_);
  }

  NfpKey key;
  key.a = a.id();
  key.b = b.id();
  key.rotation_a = a.rotation();
  key.rotation_b = b.rotation();
  key.inside = inside;

  PolygonCollection cached;
  if (cache_ && cache_->TryGet(key, &cached)) {
    return cached;
  }

  PolygonCollection result = inside ? ComputeInner(a, b) : ComputeOuter(a, b);
  result = CleanResult(result);

  if (cache_) {
    cache_->Store(key, result);
  }
  return result;
}

PolygonCollection NfpGenerator::ComputeOuter(const NestPolygon& a,
                                             const NestPolygon& b) {
  PolygonCollection base = minkowski_.ComputeAll(a.geometry(), b.geometry());

  if (!config_.use_holes()) {
    return base;
  }

  PolygonCollection exclusions = BuildHolePolygons(b);
  PolygonCollection difference;
  for (const auto& hole : exclusions) {
    PolygonCollection hole_nfp = minkowski_.ComputeAll(a.geometry(), hole);
    difference.insert(difference.end(), hole_nfp.begin(), hole_nfp.end());
  }

  if (difference.empty()) {
    return base;
  }

  return Difference(base, difference);
}

PolygonCollection NfpGenerator::ComputeInner(const NestPolygon& a,
                                             const NestPolygon& b) {
  PolygonWithHoles frame = BuildFramePolygon(a);
  if (frame.outer().size() < 3) {
    return {};
  }

  PolygonCollection base = minkowski_.ComputeAll(frame, b.geometry());
  if (!config_.use_holes()) {
    return base;
  }

  PolygonCollection hole_nfps;
  for (const auto& child : a.children()) {
    PolygonCollection child_nfp = minkowski_.ComputeAll(child.geometry(), b.geometry());
    hole_nfps.insert(hole_nfps.end(), child_nfp.begin(), child_nfp.end());
  }

  if (hole_nfps.empty()) {
    return base;
  }

  return Difference(base, hole_nfps);
}

PolygonCollection NfpGenerator::CleanResult(
    const PolygonCollection& polygons) const {
  if (polygons.empty()) {
    return polygons;
  }
  PolygonCollection cleaned = CleanPolygons(polygons, config_.endpoint_tolerance());
  if (cleaned.empty()) {
    return polygons;
  }
  return cleaned;
}

}  // namespace deepnest
