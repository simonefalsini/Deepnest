#include "../../include/deepnest/placement/PlacementStrategy.h"
#include "../../include/deepnest/config/DeepNestConfig.h"
#include "../../include/deepnest/placement/MergeDetection.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/ConvexHull.h"
#include <limits>
#include <algorithm>

namespace deepnest {

// ==================== PlacementStrategy Base ====================

std::unique_ptr<PlacementStrategy> PlacementStrategy::create(Type type) {
    switch (type) {
        case Type::GRAVITY:
            return std::make_unique<GravityPlacement>();
        case Type::BOUNDING_BOX:
            return std::make_unique<BoundingBoxPlacement>();
        case Type::CONVEX_HULL:
            return std::make_unique<ConvexHullPlacement>();
        default:
            return std::make_unique<GravityPlacement>(); // Default to gravity
    }
}

std::unique_ptr<PlacementStrategy> PlacementStrategy::create(const std::string& typeName) {
    if (typeName == "gravity") {
        return std::make_unique<GravityPlacement>();
    } else if (typeName == "box" || typeName == "bounding_box") {
        return std::make_unique<BoundingBoxPlacement>();
    } else if (typeName == "convexhull" || typeName == "convex_hull" || typeName == "hull") {
        return std::make_unique<ConvexHullPlacement>();
    } else {
        return std::make_unique<GravityPlacement>(); // Default
    }
}

// ==================== GravityPlacement ====================

BestPositionResult GravityPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions,
    const DeepNestConfig& config
) const {

    if (candidatePositions.empty()) {
        return BestPositionResult(); // No valid positions
    }

    // CRITICAL FIX: Match JavaScript tie-breaking logic exactly
    // JavaScript background.js:1099-1113 uses ABSOLUTE minimum x/y values
    // NOT the x/y of the currently selected position!
    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    double bestMergedLength = 0.0;
    bool foundValid = false;

    // These track ABSOLUTE minimum x/y values seen across ALL selected positions
    // (matching JavaScript behavior where minx/miny are updated conditionally)
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();

    // Evaluate each candidate position
    // JavaScript: for(j=0; j<finalNfp.length; j++) { for(k=0; k<nf.length; k())
    for (const auto& position : candidatePositions) {
        // Calculate base area metric
        double metric = calculateMetric(part, position, placed);

        // LINE MERGE INTEGRATION: Calculate merged line bonus
        // JavaScript background.js:1094: area -= merged.totalLength * config.timeRatio
        double mergedLength = 0.0;
        if (config.mergeLines) {
            // Create test part at this position
            Polygon testPart = part;
            for (auto& p : testPart.points) {
                p.x += position.x;
                p.y += position.y;
            }
            // Also translate children (holes)
            for (auto& child : testPart.children) {
                for (auto& p : child.points) {
                    p.x += position.x;
                    p.y += position.y;
                }
            }

            // Convert PlacedPart vector to Polygon vector for MergeDetection
            std::vector<Polygon> placedPolygons;
            for (const auto& placedPart : placed) {
                Polygon poly = placedPart.polygon;
                // Apply placement transformation
                for (auto& p : poly.points) {
                    p.x += placedPart.position.x;
                    p.y += placedPart.position.y;
                }
                for (auto& child : poly.children) {
                    for (auto& p : child.points) {
                        p.x += placedPart.position.x;
                        p.y += placedPart.position.y;
                    }
                }
                placedPolygons.push_back(poly);
            }

            // Calculate merged lines
            double minLength = 0.1; // Minimum edge length to consider
            double tolerance = 0.1 * config.curveTolerance;
            auto mergeResult = MergeDetection::calculateMergedLength(
                placedPolygons, testPart, minLength, tolerance
            );
            mergedLength = mergeResult.totalLength;

            // Apply line merge bonus (subtract from metric - lower is better)
            metric -= mergedLength * config.timeRatio;
        }

        // CRITICAL FIX: Three-level tie-breaking matching JavaScript background.js:1099-1104
        // JavaScript logic:
        // if(minarea === null ||
        //    area < minarea ||
        //    (GeometryUtil.almostEqual(minarea, area) && (minx === null || shiftvector.x < minx)) ||
        //    (GeometryUtil.almostEqual(minarea, area) && (minx !== null &&
        //     GeometryUtil.almostEqual(shiftvector.x, minx) && shiftvector.y < miny)))

        const double TOLERANCE = 0.001;  // almostEqual tolerance

        bool selectThis = false;
        if (!foundValid) {
            // First position - always select
            selectThis = true;
        } else if (metric < minMetric - TOLERANCE) {
            // Level 1: Clearly better metric (area)
            selectThis = true;
        } else if (std::abs(metric - minMetric) < TOLERANCE) {
            // Metric is approximately equal, use tie-breaking
            if (position.x < minX - TOLERANCE) {
                // Level 2: X is better (smaller)
                selectThis = true;
            } else if (std::abs(position.x - minX) < TOLERANCE && position.y < minY - TOLERANCE) {
                // Level 3: X is approximately equal, Y is better (smaller)
                selectThis = true;
            }
        }

        if (selectThis) {
            minMetric = metric;
            bestPosition = position;
            bestMergedLength = mergedLength;
            foundValid = true;

            // CRITICAL: Update minX/minY ONLY if this position has smaller values
            // This matches JavaScript: if(minx === null || shiftvector.x < minx) { minx = shiftvector.x; }
            if (position.x < minX) {
                minX = position.x;
            }
            if (position.y < minY) {
                minY = position.y;
            }
        }
    }

    // CRITICAL FIX 1.3: Return area metric (minarea component for fitness)
    // JavaScript background.js:1142: fitness += (minwidth/binarea) + minarea
    // LINE MERGE: Also return merged length for logging/debugging
    if (foundValid) {
        return BestPositionResult(bestPosition, minMetric, bestMergedLength);
    } else {
        return BestPositionResult();
    }
}

double GravityPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Handle edge case: no placed parts yet (placing first part)
    if (placed.empty()) {
        // Only the new part contributes to bounds
        BoundingBox partBounds = part.bounds();
        double metric = (partBounds.width + position.x) * 2.0 + (partBounds.height + position.y);
        return metric;
    }

    // Collect all points from placed parts
    // JavaScript: for(m=0; m<placed.length; m++) { for(n=0; n<placed[m].length; n++)
    std::vector<Point> allPoints;

    // Pre-allocate space to avoid reallocations (prevents memory fragmentation)
    size_t totalPoints = 0;
    for (const auto& placedPart : placed) {
        totalPoints += placedPart.polygon.points.size();
    }
    allPoints.reserve(totalPoints);

    // Collect translated points
    for (const auto& placedPart : placed) {
        // Validate polygon before accessing points
        if (placedPart.polygon.points.empty()) {
            continue; // Skip invalid polygons
        }

        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
    }

    // Safety check: if all polygons were invalid, treat as empty
    if (allPoints.empty()) {
        BoundingBox partBounds = part.bounds();
        double metric = (partBounds.width + position.x) * 2.0 + (partBounds.height + position.y);
        return metric;
    }

    // Get bounds of all placed parts
    // JavaScript: allbounds = GeometryUtil.getPolygonBounds(allpoints);
    BoundingBox allBounds = BoundingBox::fromPoints(allPoints);

    // Get bounds of new part
    // JavaScript: partbounds = GeometryUtil.getPolygonBounds(partpoints);
    BoundingBox partBounds = part.bounds();

    // Calculate combined bounding box
    // JavaScript: var rectbounds = GeometryUtil.getPolygonBounds([...])
    std::vector<Point> combinedPoints = {
        // All bounds points
        Point(allBounds.x, allBounds.y),
        Point(allBounds.x + allBounds.width, allBounds.y),
        Point(allBounds.x + allBounds.width, allBounds.y + allBounds.height),
        Point(allBounds.x, allBounds.y + allBounds.height),

        // Part bounds points (translated to position)
        Point(partBounds.x + position.x, partBounds.y + position.y),
        Point(partBounds.x + partBounds.width + position.x, partBounds.y + position.y),
        Point(partBounds.x + partBounds.width + position.x, partBounds.y + partBounds.height + position.y),
        Point(partBounds.x + position.x, partBounds.y + partBounds.height + position.y)
    };

    BoundingBox combinedBounds = BoundingBox::fromPoints(combinedPoints);

    // Gravity metric: weight width more to compress in gravity direction
    // JavaScript: area = rectbounds.width*2 + rectbounds.height;
    double metric = combinedBounds.width * 2.0 + combinedBounds.height;

    return metric;
}

// ==================== BoundingBoxPlacement ====================

BestPositionResult BoundingBoxPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions,
    const DeepNestConfig& config
) const {

    if (candidatePositions.empty()) {
        return BestPositionResult();
    }

    // CRITICAL FIX: Match JavaScript tie-breaking logic exactly
    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    double bestMergedLength = 0.0;
    bool foundValid = false;

    // These track ABSOLUTE minimum x/y values seen across ALL selected positions
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();

    for (const auto& position : candidatePositions) {
        double metric = calculateMetric(part, position, placed);

        // LINE MERGE: Same as GravityPlacement
        double mergedLength = 0.0;
        if (config.mergeLines) {
            Polygon testPart = part;
            for (auto& p : testPart.points) {
                p.x += position.x;
                p.y += position.y;
            }
            for (auto& child : testPart.children) {
                for (auto& p : child.points) {
                    p.x += position.x;
                    p.y += position.y;
                }
            }

            std::vector<Polygon> placedPolygons;
            for (const auto& placedPart : placed) {
                Polygon poly = placedPart.polygon;
                for (auto& p : poly.points) {
                    p.x += placedPart.position.x;
                    p.y += placedPart.position.y;
                }
                for (auto& child : poly.children) {
                    for (auto& p : child.points) {
                        p.x += placedPart.position.x;
                        p.y += placedPart.position.y;
                    }
                }
                placedPolygons.push_back(poly);
            }

            double minLength = 0.1;
            double tolerance = 0.1 * config.curveTolerance;
            auto mergeResult = MergeDetection::calculateMergedLength(
                placedPolygons, testPart, minLength, tolerance
            );
            mergedLength = mergeResult.totalLength;
            metric -= mergedLength * config.timeRatio;
        }

        // CRITICAL FIX: Three-level tie-breaking matching JavaScript
        const double TOLERANCE = 0.001;

        bool selectThis = false;
        if (!foundValid) {
            // First position - always select
            selectThis = true;
        } else if (metric < minMetric - TOLERANCE) {
            // Level 1: Clearly better metric (area)
            selectThis = true;
        } else if (std::abs(metric - minMetric) < TOLERANCE) {
            // Metric is approximately equal, use tie-breaking
            if (position.x < minX - TOLERANCE) {
                // Level 2: X is better (smaller)
                selectThis = true;
            } else if (std::abs(position.x - minX) < TOLERANCE && position.y < minY - TOLERANCE) {
                // Level 3: X is approximately equal, Y is better (smaller)
                selectThis = true;
            }
        }

        if (selectThis) {
            minMetric = metric;
            bestPosition = position;
            bestMergedLength = mergedLength;
            foundValid = true;

            // CRITICAL: Update minX/minY ONLY if this position has smaller values
            if (position.x < minX) {
                minX = position.x;
            }
            if (position.y < minY) {
                minY = position.y;
            }
        }
    }

    // CRITICAL FIX 1.3: Return area metric (minarea component for fitness)
    if (foundValid) {
        return BestPositionResult(bestPosition, minMetric, bestMergedLength);
    } else {
        return BestPositionResult();
    }
}

double BoundingBoxPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Handle edge case: no placed parts yet (placing first part)
    if (placed.empty()) {
        BoundingBox partBounds = part.bounds();
        double metric = (partBounds.width + position.x) * (partBounds.height + position.y);
        return metric;
    }

    // Same as gravity, but use normal bounding box area
    std::vector<Point> allPoints;

    // Pre-allocate space to avoid reallocations
    size_t totalPoints = 0;
    for (const auto& placedPart : placed) {
        totalPoints += placedPart.polygon.points.size();
    }
    allPoints.reserve(totalPoints);

    // Collect translated points
    for (const auto& placedPart : placed) {
        // Validate polygon before accessing points
        if (placedPart.polygon.points.empty()) {
            continue; // Skip invalid polygons
        }

        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
    }

    // Safety check: if all polygons were invalid, treat as empty
    if (allPoints.empty()) {
        BoundingBox partBounds = part.bounds();
        double metric = (partBounds.width + position.x) * (partBounds.height + position.y);
        return metric;
    }

    BoundingBox allBounds = BoundingBox::fromPoints(allPoints);
    BoundingBox partBounds = part.bounds();

    std::vector<Point> combinedPoints = {
        Point(allBounds.x, allBounds.y),
        Point(allBounds.x + allBounds.width, allBounds.y),
        Point(allBounds.x + allBounds.width, allBounds.y + allBounds.height),
        Point(allBounds.x, allBounds.y + allBounds.height),

        Point(partBounds.x + position.x, partBounds.y + position.y),
        Point(partBounds.x + partBounds.width + position.x, partBounds.y + position.y),
        Point(partBounds.x + partBounds.width + position.x, partBounds.y + partBounds.height + position.y),
        Point(partBounds.x + position.x, partBounds.y + partBounds.height + position.y)
    };

    BoundingBox combinedBounds = BoundingBox::fromPoints(combinedPoints);

    // Bounding box metric: normal area
    // JavaScript: area = rectbounds.width * rectbounds.height;
    double metric = combinedBounds.width * combinedBounds.height;

    return metric;
}

// ==================== ConvexHullPlacement ====================

BestPositionResult ConvexHullPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions,
    const DeepNestConfig& config
) const {

    if (candidatePositions.empty()) {
        return BestPositionResult();
    }

    // CRITICAL FIX: Match JavaScript tie-breaking logic exactly
    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    double bestMergedLength = 0.0;
    bool foundValid = false;

    // These track ABSOLUTE minimum x/y values seen across ALL selected positions
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();

    for (const auto& position : candidatePositions) {
        double metric = calculateMetric(part, position, placed);

        // LINE MERGE: Same as GravityPlacement
        double mergedLength = 0.0;
        if (config.mergeLines) {
            Polygon testPart = part;
            for (auto& p : testPart.points) {
                p.x += position.x;
                p.y += position.y;
            }
            for (auto& child : testPart.children) {
                for (auto& p : child.points) {
                    p.x += position.x;
                    p.y += position.y;
                }
            }

            std::vector<Polygon> placedPolygons;
            for (const auto& placedPart : placed) {
                Polygon poly = placedPart.polygon;
                for (auto& p : poly.points) {
                    p.x += placedPart.position.x;
                    p.y += placedPart.position.y;
                }
                for (auto& child : poly.children) {
                    for (auto& p : child.points) {
                        p.x += placedPart.position.x;
                        p.y += placedPart.position.y;
                    }
                }
                placedPolygons.push_back(poly);
            }

            double minLength = 0.1;
            double tolerance = 0.1 * config.curveTolerance;
            auto mergeResult = MergeDetection::calculateMergedLength(
                placedPolygons, testPart, minLength, tolerance
            );
            mergedLength = mergeResult.totalLength;
            metric -= mergedLength * config.timeRatio;
        }

        // CRITICAL FIX: Three-level tie-breaking matching JavaScript
        const double TOLERANCE = 0.001;

        bool selectThis = false;
        if (!foundValid) {
            // First position - always select
            selectThis = true;
        } else if (metric < minMetric - TOLERANCE) {
            // Level 1: Clearly better metric (area)
            selectThis = true;
        } else if (std::abs(metric - minMetric) < TOLERANCE) {
            // Metric is approximately equal, use tie-breaking
            if (position.x < minX - TOLERANCE) {
                // Level 2: X is better (smaller)
                selectThis = true;
            } else if (std::abs(position.x - minX) < TOLERANCE && position.y < minY - TOLERANCE) {
                // Level 3: X is approximately equal, Y is better (smaller)
                selectThis = true;
            }
        }

        if (selectThis) {
            minMetric = metric;
            bestPosition = position;
            bestMergedLength = mergedLength;
            foundValid = true;

            // CRITICAL: Update minX/minY ONLY if this position has smaller values
            if (position.x < minX) {
                minX = position.x;
            }
            if (position.y < minY) {
                minY = position.y;
            }
        }
    }

    // CRITICAL FIX 1.3: Return area metric (minarea component for fitness)
    if (foundValid) {
        return BestPositionResult(bestPosition, minMetric, bestMergedLength);
    } else {
        return BestPositionResult();
    }
}

double ConvexHullPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Handle edge case: no placed parts yet (placing first part)
    if (placed.empty()) {
        // Only the new part contributes to area
        double area = std::abs(GeometryUtil::polygonArea(part.points));
        return area;
    }

    // Collect all points from placed parts
    // JavaScript: allpoints = getHull(allpoints);
    std::vector<Point> allPoints;

    // Pre-allocate space to avoid reallocations
    size_t totalPoints = 0;
    for (const auto& placedPart : placed) {
        totalPoints += placedPart.polygon.points.size();
    }
    allPoints.reserve(totalPoints);

    // Collect translated points
    for (const auto& placedPart : placed) {
        // Validate polygon before accessing points
        if (placedPart.polygon.points.empty()) {
            continue; // Skip invalid polygons
        }

        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
    }

    // Safety check: if all polygons were invalid, treat as empty
    if (allPoints.empty()) {
        double area = std::abs(GeometryUtil::polygonArea(part.points));
        return area;
    }

    // Calculate convex hull of placed parts
    // JavaScript: allpoints = getHull(allpoints);
    std::vector<Point> hull = ConvexHull::computeHull(allPoints);

    // Add part points at candidate position
    // JavaScript: var localpoints = clone(allpoints);
    //   for(m=0; m<part.length; m++) { localpoints.push(...) }
    std::vector<Point> combinedPoints = hull;
    for (const auto& pt : part.points) {
        combinedPoints.push_back(Point(pt.x + position.x, pt.y + position.y));
    }

    // Calculate convex hull of combined points
    // JavaScript: localpoints = getHull(localpoints);
    std::vector<Point> combinedHull = ConvexHull::computeHull(combinedPoints);

    // Calculate area of convex hull
    // JavaScript: area = -GeometryUtil.polygonArea(localpoints);
    double area = std::abs(GeometryUtil::polygonArea(combinedHull));

    return area;
}

} // namespace deepnest
