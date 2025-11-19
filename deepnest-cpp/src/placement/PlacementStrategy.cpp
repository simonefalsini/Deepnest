#include "../../include/deepnest/placement/PlacementStrategy.h"
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

Point GravityPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return Point(); // No valid positions
    }

    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    bool foundValid = false;

    // Evaluate each candidate position
    // JavaScript: for(j=0; j<finalNfp.length; j++) { for(k=0; k<nf.length; k++)
    for (const auto& position : candidatePositions) {
        double metric = calculateMetric(part, position, placed);

        if (metric < minMetric) {
            minMetric = metric;
            bestPosition = position;
            foundValid = true;
        }
    }

    return foundValid ? bestPosition : Point();
}

PlacementMetrics GravityPlacement::findBestPositionWithMetrics(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return PlacementMetrics(); // Invalid result
    }

    PlacementMetrics bestMetrics;

    // Evaluate each candidate position
    for (const auto& position : candidatePositions) {
        double width = 0.0;
        double area = 0.0;
        double metric = calculateMetricWithDetails(part, position, placed, width, area);

        if (metric < bestMetrics.metric) {
            bestMetrics = PlacementMetrics(position, metric, width, area);
        }
    }

    return bestMetrics;
}

double GravityPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Collect all points from placed parts
    // JavaScript: for(m=0; m<placed.length; m++) { for(n=0; n<placed[m].length; n++)
    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
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

double GravityPlacement::calculateMetricWithDetails(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed,
    double& outWidth,
    double& outArea
) const {

    // Collect all points from placed parts
    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
    }

    // Get bounds
    BoundingBox allBounds = BoundingBox::fromPoints(allPoints);
    BoundingBox partBounds = part.bounds();

    // Calculate combined bounding box
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

    // Output values for fitness tracking
    // JavaScript: minwidth = rectbounds.width, minarea = area
    outWidth = combinedBounds.width;
    outArea = combinedBounds.width * 2.0 + combinedBounds.height; // Same as metric for gravity

    // Gravity metric: width*2 + height
    double metric = outArea;

    return metric;
}

// ==================== BoundingBoxPlacement ====================

Point BoundingBoxPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return Point();
    }

    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    bool foundValid = false;

    for (const auto& position : candidatePositions) {
        double metric = calculateMetric(part, position, placed);

        if (metric < minMetric) {
            minMetric = metric;
            bestPosition = position;
            foundValid = true;
        }
    }

    return foundValid ? bestPosition : Point();
}

PlacementMetrics BoundingBoxPlacement::findBestPositionWithMetrics(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return PlacementMetrics();
    }

    PlacementMetrics bestMetrics;

    for (const auto& position : candidatePositions) {
        double width = 0.0;
        double area = 0.0;
        double metric = calculateMetricWithDetails(part, position, placed, width, area);

        if (metric < bestMetrics.metric) {
            bestMetrics = PlacementMetrics(position, metric, width, area);
        }
    }

    return bestMetrics;
}

double BoundingBoxPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Same as gravity, but use normal bounding box area
    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
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

double BoundingBoxPlacement::calculateMetricWithDetails(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed,
    double& outWidth,
    double& outArea
) const {

    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
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

    // Output values
    outWidth = combinedBounds.width;
    outArea = combinedBounds.width * combinedBounds.height; // Same as metric for bounding box

    double metric = outArea;

    return metric;
}

// ==================== ConvexHullPlacement ====================

Point ConvexHullPlacement::findBestPosition(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return Point();
    }

    double minMetric = std::numeric_limits<double>::max();
    Point bestPosition;
    bool foundValid = false;

    for (const auto& position : candidatePositions) {
        double metric = calculateMetric(part, position, placed);

        if (metric < minMetric) {
            minMetric = metric;
            bestPosition = position;
            foundValid = true;
        }
    }

    return foundValid ? bestPosition : Point();
}

PlacementMetrics ConvexHullPlacement::findBestPositionWithMetrics(
    const Polygon& part,
    const std::vector<PlacedPart>& placed,
    const std::vector<Point>& candidatePositions
) const {

    if (candidatePositions.empty()) {
        return PlacementMetrics();
    }

    PlacementMetrics bestMetrics;

    for (const auto& position : candidatePositions) {
        double width = 0.0;
        double area = 0.0;
        double metric = calculateMetricWithDetails(part, position, placed, width, area);

        if (metric < bestMetrics.metric) {
            bestMetrics = PlacementMetrics(position, metric, width, area);
        }
    }

    return bestMetrics;
}

double ConvexHullPlacement::calculateMetric(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed
) const {

    // Collect all points from placed parts
    // JavaScript: allpoints = getHull(allpoints);
    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
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

double ConvexHullPlacement::calculateMetricWithDetails(
    const Polygon& part,
    const Point& position,
    const std::vector<PlacedPart>& placed,
    double& outWidth,
    double& outArea
) const {

    std::vector<Point> allPoints;
    for (const auto& placedPart : placed) {
        for (const auto& pt : placedPart.polygon.points) {
            allPoints.push_back(Point(pt.x + placedPart.position.x,
                                     pt.y + placedPart.position.y));
        }
    }

    std::vector<Point> hull = ConvexHull::computeHull(allPoints);

    std::vector<Point> combinedPoints = hull;
    for (const auto& pt : part.points) {
        combinedPoints.push_back(Point(pt.x + position.x, pt.y + position.y));
    }

    std::vector<Point> combinedHull = ConvexHull::computeHull(combinedPoints);

    // Calculate hull bounding box for width
    BoundingBox hullBounds = BoundingBox::fromPoints(combinedHull);
    outWidth = hullBounds.width;

    // Area is the metric itself for convex hull
    outArea = std::abs(GeometryUtil::polygonArea(combinedHull));

    return outArea;
}

} // namespace deepnest
