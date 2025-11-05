#pragma once

#include "deepnest/NfpGenerator.h"
#include "deepnest/PlacementCostEvaluator.h"

#include <vector>

namespace deepnest {

class PlacementWorker {
 public:
  PlacementWorker(NfpGenerator* generator, const DeepNestConfig& config);

  std::vector<NestPolygon> Place(const std::vector<NestPolygon>& parts);

 private:
  NfpGenerator* generator_;
  const DeepNestConfig& config_;
  PlacementCostEvaluator evaluator_;
};

}  // namespace deepnest
