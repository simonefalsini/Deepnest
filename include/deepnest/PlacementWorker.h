#pragma once

#include "deepnest/NfpGenerator.h"
#include "deepnest/PlacementCostEvaluator.h"

#include <limits>
#include <optional>
#include <vector>

namespace deepnest {

struct PlacementResult {
  std::vector<NestPolygon> placements;
  std::vector<NestPolygon> unplaced;
  PlacementCost cost;
};

class PlacementWorker {
 public:
  PlacementWorker(NfpGenerator* generator, const DeepNestConfig& config);

  void SetSheets(const std::vector<NestPolygon>& sheets);

  PlacementResult Place(const std::vector<NestPolygon>& parts);

 private:
  struct CandidatePlacement {
    QPointF position;
    double score {std::numeric_limits<double>::infinity()};
    double merged_length {0.0};
  };

  static Loop ExtractLoop(const Polygon& polygon);
  static LoopCollection ExtractHoles(const PolygonWithHoles& polygon);
  static Loop TranslateLoop(const Loop& loop, const QPointF& delta);
  static BoundingBox TranslateBounds(const BoundingBox& bounds,
                                     const QPointF& delta);

  std::optional<CandidatePlacement> FindBestPlacement(
      const NestPolygon& sheet, const NestPolygon& part,
      const std::vector<NestPolygon>& placed,
      const std::vector<QPointF>& placements) const;

  Coordinate ComputeUnplacedArea(const std::vector<NestPolygon>& parts) const;
  double ComputeMergedLength(const NestPolygon& candidate,
                             const QPointF& candidate_position,
                             const std::vector<NestPolygon>& placed,
                             const std::vector<QPointF>& placements) const;

  NfpGenerator* generator_;
  const DeepNestConfig& config_;
  PlacementCostEvaluator evaluator_;
  std::vector<NestPolygon> sheets_;
};

}  // namespace deepnest
