#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/GeometryUtils.h"
#include "deepnest/NestPolygon.h"

#include <vector>

namespace deepnest {

struct PlacementCost {
  BoundingBox bounding_box;
  double used_area {0.0};
  double sheet_area {0.0};
  double bounding_area {0.0};
  double waste {0.0};
  double merged_length {0.0};
  double fitness {0.0};
};

class PlacementCostEvaluator {
 public:
  explicit PlacementCostEvaluator(const DeepNestConfig& config);

  PlacementCost Evaluate(const NestPolygon& sheet,
                         const std::vector<NestPolygon>& placed_parts,
                         double merged_length,
                         double unplaced_area) const;

 private:
  Loop CollectHullPoints(const std::vector<NestPolygon>& placed_parts) const;

  const DeepNestConfig& config_;
};

}  // namespace deepnest
