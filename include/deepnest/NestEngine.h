#pragma once

#include "deepnest/GeneticAlgorithm.h"
#include "deepnest/NestPartRepository.h"
#include "deepnest/PlacementWorker.h"
#include "deepnest/ThreadPool.h"

#include <QHash>
#include <QPainterPath>

#include <functional>
#include <memory>
#include <vector>

namespace deepnest {

class NestEngine {
 public:
  explicit NestEngine(const DeepNestConfig& config);

  void Initialize(const QPainterPath& sheet,
                  const QHash<QString, QPainterPath>& parts);
  PlacementResult Run();
  PlacementResult RunWithParts(const std::vector<NestPolygon>& parts);

  void UpdateConfig(const DeepNestConfig& config);
  void SetProgressCallback(GeneticAlgorithm::ProgressCallback callback);
  void ClearCache();
  void Reset();

  const std::vector<NestPolygon>& parts() const { return parts_; }

 private:
  std::vector<double> CandidateRotations() const;
  void PrecomputeNfps();
  void RebuildComponents();
  void ConfigureThreadPool();

  DeepNestConfig config_;
  NfpCache cache_;
  NfpGenerator generator_;
  NestPartRepository repository_;
  PlacementWorker worker_;
  GeneticAlgorithm algorithm_;
  std::unique_ptr<ThreadPool> thread_pool_;
  GeneticAlgorithm::ProgressCallback progress_callback_;
  std::vector<NestPolygon> parts_;
  NestPolygon sheet_;
};

}  // namespace deepnest
