#pragma once

#include "deepnest/GeneticAlgorithm.h"
#include "deepnest/NestPartRepository.h"
#include "deepnest/PlacementWorker.h"

#include <QHash>
#include <QPainterPath>

#include <vector>

namespace deepnest {

class NestEngine {
 public:
  explicit NestEngine(const DeepNestConfig& config);

  void Initialize(const QPainterPath& sheet,
                  const QHash<QString, QPainterPath>& parts);
  std::vector<NestPolygon> Run();

 private:
  DeepNestConfig config_;
  NfpCache cache_;
  NfpGenerator generator_;
  NestPartRepository repository_;
  PlacementWorker worker_;
  GeneticAlgorithm algorithm_;
  std::vector<NestPolygon> parts_;
};

}  // namespace deepnest
