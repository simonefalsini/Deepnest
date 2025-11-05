#pragma once

#include "deepnest/DeepNestConfig.h"
#include "deepnest/NestEngine.h"

#include <QHash>
#include <QList>
#include <QPair>
#include <QPainterPath>
#include <QPointF>
#include <QString>

namespace deepnest {

class DeepNestSolver {
 public:
  DeepNestSolver();

  void SetConfig(const DeepNestConfig& config);
  QList<QHash<QString, QPair<QPointF, double>>> Solve(
      const QPainterPath& sheet,
      const QHash<QString, QPainterPath>& parts);

 private:
  DeepNestConfig config_;
};

}  // namespace deepnest
