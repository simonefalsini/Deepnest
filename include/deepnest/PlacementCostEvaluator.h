#pragma once

#include "deepnest/GeometryUtils.h"
#include "deepnest/NestPolygon.h"

#include <vector>

namespace deepnest {

struct PlacementCost {
  double area {0.0};
  double waste {0.0};
};

class PlacementCostEvaluator {
 public:
  PlacementCost Evaluate(const std::vector<NestPolygon>& placed_parts) const;
};

}  // namespace deepnest
