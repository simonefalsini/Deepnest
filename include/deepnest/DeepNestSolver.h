#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/NestEngine.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace deepnest {

class DeepNestSolver {
 public:
  using ProgressCallback = GeneticAlgorithm::ProgressCallback;

  DeepNestSolver();

  const DeepNestConfig& config() const { return config_; }
  void SetConfig(const DeepNestConfig& config);
  void SetProgressCallback(ProgressCallback callback);
  void ClearProgressCallback();

  QList<QHash<QString, QPair<QPointF, double>>> Solve(
      const QPainterPath& sheet,
      const QHash<QString, QPainterPath>& parts);

  const QList<QHash<QString, QPair<QPointF, double>>>& last_result() const {
    return last_result_;
  }

  const QStringList& unplaced_ids() const { return unplaced_ids_; }

  void Reset();
  void ClearCache();

 private:
  void EnsureEngine();

  DeepNestConfig config_;
  bool config_dirty_ {false};
  std::unique_ptr<NestEngine> engine_;
  ProgressCallback progress_callback_;
  QList<QHash<QString, QPair<QPointF, double>>> last_result_;
  QStringList unplaced_ids_;
};

}  // namespace deepnest
