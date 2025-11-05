#pragma once

#include "deepnest/PlacementWorker.h"

#include <vector>

namespace deepnest {

struct Individual {
  std::vector<int> order;
  double fitness {0.0};
};

class GeneticAlgorithm {
 public:
  GeneticAlgorithm(const DeepNestConfig& config, PlacementWorker* worker);

  Individual Run(const std::vector<NestPolygon>& parts);

 private:
  const DeepNestConfig& config_;
  PlacementWorker* worker_;
};

}  // namespace deepnest
