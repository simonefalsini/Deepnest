#include "deepnest/PlacementCostEvaluator.h"

#include <algorithm>
#include <limits>

#include <QPointF>
#include <QString>

namespace deepnest {

namespace {

BoundingBox TranslateBoundingBox(const BoundingBox& box, const QPointF& delta) {
  if (box.IsEmpty()) {
    return box;
  }
  BoundingBox translated = box;
  translated.min_x += delta.x();
  translated.max_x += delta.x();
  translated.min_y += delta.y();
  translated.max_y += delta.y();
  return translated;
}

}  // namespace

PlacementCostEvaluator::PlacementCostEvaluator(const DeepNestConfig& config)
    : config_(config) {}

Loop PlacementCostEvaluator::CollectHullPoints(
    const std::vector<NestPolygon>& placed_parts) const {
  Loop all_points;
  for (const auto& part : placed_parts) {
    const auto& outer = Outer(part.geometry());
    for (const auto& point : outer) {
      const Coordinate x = point.x() + part.position().x();
      const Coordinate y = point.y() + part.position().y();
      all_points.emplace_back(x, y);
    }
  }
  return all_points;
}

PlacementCost PlacementCostEvaluator::Evaluate(
    const NestPolygon& sheet, const std::vector<NestPolygon>& placed_parts,
    double merged_length, double unplaced_area) const {
  PlacementCost cost;
  cost.merged_length = merged_length;
  cost.sheet_area = std::abs(ComputeArea(sheet.geometry()));

  BoundingBox aggregate_bounds = BoundingBox::Empty();
  Coordinate used_area = 0.0;
  for (const auto& part : placed_parts) {
    used_area += std::abs(ComputeArea(part.geometry()));
    BoundingBox box = ComputeBoundingBox(part.geometry());
    aggregate_bounds.Expand(TranslateBoundingBox(box, part.position()));
  }

  cost.bounding_box = aggregate_bounds;
  cost.used_area = used_area;
  cost.bounding_area = aggregate_bounds.Width() * aggregate_bounds.Height();
  cost.waste = std::max(0.0, cost.bounding_area - used_area);

  double fitness = 0.0;
  if (config_.placement_type() == QStringLiteral("gravity")) {
    fitness = aggregate_bounds.Width() * 2.0 + aggregate_bounds.Height();
  } else if (config_.placement_type() == QStringLiteral("box")) {
    fitness = cost.bounding_area;
  } else {
    Loop hull_points = CollectHullPoints(placed_parts);
    if (!hull_points.empty()) {
      const Loop hull = ComputeConvexHull(hull_points);
      fitness = std::abs(ComputeArea(hull));
    }
  }

  if (config_.merge_lines()) {
    fitness -= merged_length * config_.time_ratio();
  }

  if (unplaced_area > 0.0 && cost.sheet_area > 0.0) {
    fitness += 100000000.0 * (unplaced_area / cost.sheet_area);
  }

  cost.fitness = fitness;
  return cost;
}

}  // namespace deepnest
