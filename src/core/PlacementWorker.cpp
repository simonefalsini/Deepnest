#include "deepnest/PlacementWorker.h"

namespace deepnest {

PlacementWorker::PlacementWorker(NfpGenerator* generator,
                                 const DeepNestConfig& config)
    : generator_(generator), config_(config) {}

std::vector<NestPolygon> PlacementWorker::Place(
    const std::vector<NestPolygon>& parts) {
  // Placeholder: simply returns input.
  std::vector<NestPolygon> placed = parts;
  (void)generator_;
  (void)config_;
  evaluator_.Evaluate(placed);
  return placed;
}

}  // namespace deepnest
