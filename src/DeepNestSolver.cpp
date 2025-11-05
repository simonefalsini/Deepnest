#include "deepnest/DeepNestSolver.h"

#include <utility>
#include <vector>

namespace deepnest {

DeepNestSolver::DeepNestSolver() = default;

void DeepNestSolver::SetConfig(const DeepNestConfig& config) {
  if (config_ == config) {
    return;
  }
  config_ = config;
  config_dirty_ = true;
}

void DeepNestSolver::SetProgressCallback(ProgressCallback callback) {
  progress_callback_ = std::move(callback);
  if (engine_) {
    engine_->SetProgressCallback(progress_callback_);
  }
}

void DeepNestSolver::ClearProgressCallback() {
  progress_callback_ = ProgressCallback();
  if (engine_) {
    engine_->SetProgressCallback(progress_callback_);
  }
}

void DeepNestSolver::Reset() {
  last_result_.clear();
  unplaced_ids_.clear();
  if (engine_) {
    engine_->Reset();
  }
}

void DeepNestSolver::ClearCache() {
  if (engine_) {
    engine_->ClearCache();
  }
}

void DeepNestSolver::EnsureEngine() {
  if (!engine_) {
    engine_ = std::make_unique<NestEngine>(config_);
    if (progress_callback_) {
      engine_->SetProgressCallback(progress_callback_);
    }
    config_dirty_ = false;
    return;
  }

  if (config_dirty_) {
    engine_->UpdateConfig(config_);
    if (progress_callback_) {
      engine_->SetProgressCallback(progress_callback_);
    }
    config_dirty_ = false;
  }
}

QList<QHash<QString, QPair<QPointF, double>>> DeepNestSolver::Solve(
    const QPainterPath& sheet, const QHash<QString, QPainterPath>& parts) {
  EnsureEngine();
  if (!engine_) {
    return {};
  }

  engine_->Initialize(sheet, parts);

  QList<QHash<QString, QPair<QPointF, double>>> result;
  last_result_.clear();
  unplaced_ids_.clear();

  std::vector<NestPolygon> pending = engine_->parts();
  std::vector<NestPolygon> leftover = pending;

  while (!pending.empty()) {
    PlacementResult placement = engine_->RunWithParts(pending);
    if (placement.placements.empty()) {
      leftover = pending;
      break;
    }

    QHash<QString, QPair<QPointF, double>> sheet_result;
    sheet_result.reserve(static_cast<int>(placement.placements.size()));
    for (const auto& part : placement.placements) {
      // La posizione corrisponde alla traslazione applicata al primo vertice
      // dell'anello esterno, mantenendo la rotazione accumulata in NestPolygon.
      sheet_result.insert(part.id(), qMakePair(part.position(), part.rotation()));
    }

    if (!sheet_result.isEmpty()) {
      result.append(sheet_result);
    }

    if (placement.unplaced.empty()) {
      leftover.clear();
      break;
    }

    if (placement.unplaced.size() == pending.size()) {
      leftover = placement.unplaced;
      break;
    }

    pending = placement.unplaced;
    leftover = pending;
  }

  for (const auto& part : leftover) {
    unplaced_ids_.append(part.id());
  }

  last_result_ = result;
  return result;
}

}  // namespace deepnest
