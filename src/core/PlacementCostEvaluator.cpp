#include "deepnest/PlacementCostEvaluator.h"

namespace deepnest {

PlacementCost PlacementCostEvaluator::Evaluate(
    const std::vector<NestPolygon>& placed_parts) const {
  PlacementCost cost;
  for (const auto& part : placed_parts) {
    cost.area += ComputeArea(part.geometry());
  }
  cost.waste = 0.0;
  return cost;
}

}  // namespace deepnest
