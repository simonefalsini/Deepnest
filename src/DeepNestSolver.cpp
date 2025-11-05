#include "deepnest/DeepNestSolver.h"

namespace deepnest {

DeepNestSolver::DeepNestSolver() : config_() {}

void DeepNestSolver::SetConfig(const DeepNestConfig& config) {
  config_ = config;
}

QList<QHash<QString, QPair<QPointF, double>>> DeepNestSolver::Solve(
    const QPainterPath& sheet, const QHash<QString, QPainterPath>& parts) {
  NestEngine engine(config_);
  engine.Initialize(sheet, parts);
  auto placed = engine.Run();

  QList<QHash<QString, QPair<QPointF, double>>> result;
  QHash<QString, QPair<QPointF, double>> sheet_result;
  for (const auto& part : placed) {
    sheet_result.insert(part.id(), qMakePair(part.position(), part.rotation()));
  }
  result.append(sheet_result);
  return result;
}

}  // namespace deepnest
