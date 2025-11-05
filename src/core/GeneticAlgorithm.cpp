#include "deepnest/GeneticAlgorithm.h"

#include <numeric>

namespace deepnest {

GeneticAlgorithm::GeneticAlgorithm(const DeepNestConfig& config,
                                   PlacementWorker* worker)
    : config_(config), worker_(worker) {}

Individual GeneticAlgorithm::Run(const std::vector<NestPolygon>& parts) {
  Individual best;
  best.order.resize(parts.size());
  std::iota(best.order.begin(), best.order.end(), 0);
  if (worker_) {
    auto placed = worker_->Place(parts);
    best.fitness = static_cast<double>(placed.size());
  }
  return best;
}

}  // namespace deepnest
