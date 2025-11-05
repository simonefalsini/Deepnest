#pragma once

#include "deepnest/PlacementWorker.h"
#include "deepnest/ThreadPool.h"

#include <functional>
#include <random>
#include <vector>

namespace deepnest {

struct Individual {
  std::vector<int> order;
  std::vector<double> rotations;
  PlacementResult result;
  bool evaluated {false};
};

class GeneticAlgorithm {
 public:
  using ProgressCallback = std::function<void(int, const PlacementResult&)>;

  GeneticAlgorithm(const DeepNestConfig& config, PlacementWorker* worker);

  PlacementResult Run(const std::vector<NestPolygon>& parts);

  void set_thread_pool(ThreadPool* thread_pool) { thread_pool_ = thread_pool; }
  void set_progress_callback(ProgressCallback callback);

 private:
  Individual CreateSeed(const std::vector<int>& base_order) const;
  Individual Mutate(const Individual& individual);
  PlacementResult Evaluate(Individual* individual,
                           const std::vector<NestPolygon>& parts);
  std::vector<NestPolygon> BuildArrangement(
      const Individual& individual, const std::vector<NestPolygon>& parts) const;
  double MutationProbability() const;
  double RandomRotation() const;
  std::vector<double> GenerateRotations(std::size_t count) const;

  const DeepNestConfig& config_;
  PlacementWorker* worker_;
  mutable std::mt19937 random_;
  std::vector<Individual> population_;
  ThreadPool* thread_pool_ {nullptr};
  ProgressCallback progress_callback_;
};

}  // namespace deepnest
