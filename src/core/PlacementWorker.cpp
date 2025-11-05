#include "deepnest/PlacementWorker.h"

#include "deepnest/ClipperBridge.h"
#include "deepnest/GeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QString>

namespace deepnest {

namespace {

clipper2::PathsD TranslatePaths(const clipper2::PathsD& paths,
                                const QPointF& delta) {
  clipper2::PathsD translated = paths;
  for (auto& path : translated) {
    for (auto& point : path) {
      point.x += delta.x();
      point.y += delta.y();
    }
  }
  return translated;
}

clipper2::PathsD UnionPaths(const clipper2::PathsD& base,
                            const clipper2::PathsD& addition) {
  if (base.empty()) {
    return addition;
  }
  if (addition.empty()) {
    return base;
  }

  clipper2::ClipperD clipper;
  clipper.AddSubject(base);
  clipper.AddSubject(addition);
  clipper2::PathsD result;
  clipper.Execute(clipper2::ClipType::Union, clipper2::FillRule::NonZero,
                  result);
  return result;
}

clipper2::PathsD DifferencePaths(const clipper2::PathsD& subject,
                                 const clipper2::PathsD& clip) {
  if (subject.empty()) {
    return {};
  }

  if (clip.empty()) {
    return subject;
  }

  clipper2::ClipperD clipper;
  clipper.AddSubject(subject);
  clipper.AddClip(clip);
  clipper2::PathsD result;
  clipper.Execute(clipper2::ClipType::Difference,
                  clipper2::FillRule::NonZero, result);
  return result;
}

double EdgeLength(const Point& a, const Point& b) {
  return std::hypot(a.x() - b.x(), a.y() - b.y());
}

bool IsEdgeExact(const NestPolygon& polygon, std::size_t index) {
  const auto& flags = polygon.outer_vertex_exact();
  if (flags.empty() || flags.size() <= index) {
    return polygon.exact();
  }
  const std::size_t next = (index + 1) % flags.size();
  return flags[index] && flags[next];
}

double OverlapLength(const Point& a1, const Point& a2, const Point& b1,
                     const Point& b2, double tolerance) {
  const double dx = a2.x() - a1.x();
  const double dy = a2.y() - a1.y();
  const double length = std::hypot(dx, dy);
  if (length <= tolerance) {
    return 0.0;
  }

  const double ux = dx / length;
  const double uy = dy / length;

  const auto project = [&](const Point& p) {
    return ((p.x() - a1.x()) * ux + (p.y() - a1.y()) * uy);
  };

  const double proj_b1 = project(b1);
  const double proj_b2 = project(b2);

  const double cross1 = std::abs((b1.x() - a1.x()) * dy -
                                 (b1.y() - a1.y()) * dx);
  const double cross2 = std::abs((b2.x() - a1.x()) * dy -
                                 (b2.y() - a1.y()) * dx);
  if (cross1 > tolerance * length || cross2 > tolerance * length) {
    return 0.0;
  }

  const double min_proj = std::max(0.0, std::min(proj_b1, proj_b2));
  const double max_proj = std::min(length, std::max(proj_b1, proj_b2));
  if (max_proj <= min_proj) {
    return 0.0;
  }
  return max_proj - min_proj;
}

}  // namespace

PlacementWorker::PlacementWorker(NfpGenerator* generator,
                                 const DeepNestConfig& config)
    : generator_(generator),
      config_(config),
      evaluator_(config) {}

void PlacementWorker::SetSheets(const std::vector<NestPolygon>& sheets) {
  sheets_ = sheets;
}

Loop PlacementWorker::ExtractLoop(const Polygon& polygon) {
  Loop loop;
  loop.reserve(polygon.size());
  for (const auto& point : polygon) {
    loop.emplace_back(point.x(), point.y());
  }
  if (!loop.empty() && AlmostEqual(loop.front().x(), loop.back().x()) &&
      AlmostEqual(loop.front().y(), loop.back().y())) {
    loop.pop_back();
  }
  return loop;
}

LoopCollection PlacementWorker::ExtractHoles(
    const PolygonWithHoles& polygon) {
  LoopCollection holes;
  for (const auto& inner : Holes(polygon)) {
    holes.emplace_back(ExtractLoop(inner));
  }
  return holes;
}

Loop PlacementWorker::TranslateLoop(const Loop& loop, const QPointF& delta) {
  Loop translated;
  translated.reserve(loop.size());
  for (const auto& point : loop) {
    translated.emplace_back(point.x() + delta.x(),
                            point.y() + delta.y());
  }
  return translated;
}

BoundingBox PlacementWorker::TranslateBounds(const BoundingBox& bounds,
                                             const QPointF& delta) {
  if (bounds.IsEmpty()) {
    return bounds;
  }
  BoundingBox translated = bounds;
  translated.min_x += delta.x();
  translated.max_x += delta.x();
  translated.min_y += delta.y();
  translated.max_y += delta.y();
  return translated;
}

Coordinate PlacementWorker::ComputeUnplacedArea(
    const std::vector<NestPolygon>& parts) const {
  Coordinate total = 0.0;
  for (const auto& part : parts) {
    total += std::abs(ComputeArea(part.geometry()));
  }
  return total;
}

double PlacementWorker::ComputeMergedLength(
    const NestPolygon& candidate, const QPointF& candidate_position,
    const std::vector<NestPolygon>& placed,
    const std::vector<QPointF>& placements) const {
  if (!config_.merge_lines()) {
    return 0.0;
  }

  const double min_length = 0.5 * config_.scale();
  const double tolerance = 0.1 * config_.curve_tolerance();

  const Loop candidate_loop =
      TranslateLoop(ExtractLoop(Outer(candidate.geometry())), candidate_position);

  double merged = 0.0;
  if (candidate_loop.size() < 2) {
    return merged;
  }

  for (std::size_t i = 0; i < candidate_loop.size(); ++i) {
    const std::size_t next = (i + 1) % candidate_loop.size();
    if (!IsEdgeExact(candidate, i)) {
      continue;
    }

    const Point& a1 = candidate_loop[i];
    const Point& a2 = candidate_loop[next];
    const double length = EdgeLength(a1, a2);
    if (length < min_length) {
      continue;
    }

    for (std::size_t p = 0; p < placed.size(); ++p) {
      const Loop placed_loop = TranslateLoop(
          ExtractLoop(Outer(placed[p].geometry())), placements[p]);
      if (placed_loop.size() < 2) {
        continue;
      }
      for (std::size_t j = 0; j < placed_loop.size(); ++j) {
        if (!IsEdgeExact(placed[p], j)) {
          continue;
        }
        const std::size_t jnext = (j + 1) % placed_loop.size();
        const Point& b1 = placed_loop[j];
        const Point& b2 = placed_loop[jnext];
        merged += OverlapLength(a1, a2, b1, b2, tolerance);
      }
    }
  }

  return merged;
}

std::optional<PlacementWorker::CandidatePlacement>
PlacementWorker::FindBestPlacement(
    const NestPolygon& sheet, const NestPolygon& part,
    const std::vector<NestPolygon>& placed,
    const std::vector<QPointF>& placements) const {
  if (!generator_) {
    return std::nullopt;
  }

  PolygonCollection sheet_nfp = generator_->Compute(sheet, part, /*inside=*/true);
  if (sheet_nfp.empty()) {
    return std::nullopt;
  }

  clipper2::PathsD sheet_paths = PolygonCollectionToClipper(sheet_nfp);
  clipper2::PathsD forbidden;
  if (!placed.empty()) {
    for (std::size_t i = 0; i < placed.size(); ++i) {
      PolygonCollection outer = generator_->Compute(placed[i], part, /*inside=*/false);
      clipper2::PathsD outer_paths = PolygonCollectionToClipper(outer);
      clipper2::PathsD shifted = TranslatePaths(outer_paths, placements[i]);
      forbidden = UnionPaths(forbidden, shifted);
    }
  }

  clipper2::PathsD available = DifferencePaths(sheet_paths, forbidden);
  if (available.empty()) {
    available = sheet_paths;
  }

  const Loop part_loop = ExtractLoop(Outer(part.geometry()));
  if (part_loop.empty()) {
    return std::nullopt;
  }

  BoundingBox existing_bounds = BoundingBox::Empty();
  Loop existing_points;
  for (std::size_t i = 0; i < placed.size(); ++i) {
    Loop loop = TranslateLoop(ExtractLoop(Outer(placed[i].geometry())),
                              placements[i]);
    for (const auto& point : loop) {
      existing_points.push_back(point);
      existing_bounds.Expand(point);
    }
  }

  CandidatePlacement best;

  for (const auto& path : available) {
    Loop loop = ClipperToLoop(path);
    for (const auto& point : loop) {
      const QPointF shift(point.x(), point.y());
      const QPointF offset(shift.x() - part_loop.front().x(),
                           shift.y() - part_loop.front().y());

      Loop candidate_points = TranslateLoop(part_loop, offset);
      BoundingBox candidate_bounds = ComputeBoundingBox(part.geometry());
      BoundingBox combined = existing_bounds;
      combined.Expand(TranslateBounds(candidate_bounds, offset));

      double metric = 0.0;
      if (config_.placement_type() == QStringLiteral("gravity")) {
        metric = combined.Width() * 2.0 + combined.Height();
      } else if (config_.placement_type() == QStringLiteral("box")) {
        metric = combined.Width() * combined.Height();
      } else {
        Loop points = existing_points;
        points.insert(points.end(), candidate_points.begin(),
                      candidate_points.end());
        if (!points.empty()) {
          Loop hull = ComputeConvexHull(points);
          metric = std::abs(ComputeArea(hull));
        }
      }

      double merged = ComputeMergedLength(part, offset, placed, placements);
      if (config_.merge_lines()) {
        metric -= merged * config_.time_ratio();
      }

      if (metric < best.score ||
          (AlmostEqual(metric, best.score) &&
           (offset.x() < best.position.x() ||
            (AlmostEqual(offset.x(), best.position.x()) &&
             offset.y() < best.position.y())))) {
        best.score = metric;
        best.position = offset;
        best.merged_length = merged;
      }
    }
  }

  if (!std::isfinite(best.score)) {
    return std::nullopt;
  }

  return best;
}

PlacementResult PlacementWorker::Place(
    const std::vector<NestPolygon>& parts) {
  PlacementResult result;
  if (sheets_.empty()) {
    result.unplaced = parts;
    return result;
  }

  const NestPolygon& sheet = sheets_.front();

  std::vector<NestPolygon> remaining;
  remaining.reserve(parts.size());
  for (const auto& part : parts) {
    remaining.push_back(part.Clone());
  }

  std::vector<NestPolygon> placed;
  std::vector<QPointF> placements;
  double total_merged = 0.0;

  while (!remaining.empty()) {
    bool placed_in_iteration = false;
    for (auto it = remaining.begin(); it != remaining.end();) {
      const auto candidate = FindBestPlacement(sheet, *it, placed, placements);
      if (!candidate.has_value()) {
        ++it;
        continue;
      }

      NestPolygon positioned = it->Clone();
      positioned.set_position(candidate->position);
      placed.push_back(positioned);
      placements.push_back(candidate->position);
      total_merged += candidate->merged_length;
      it = remaining.erase(it);
      placed_in_iteration = true;
    }

    if (!placed_in_iteration) {
      break;
    }
  }

  result.placements = placed;
  result.unplaced = remaining;
  result.cost = evaluator_.Evaluate(sheet, result.placements, total_merged,
                                    ComputeUnplacedArea(result.unplaced));
  return result;
}

}  // namespace deepnest
