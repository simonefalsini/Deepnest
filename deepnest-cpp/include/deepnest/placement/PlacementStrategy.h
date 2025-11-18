#ifndef DEEPNEST_PLACEMENT_STRATEGY_H
#define DEEPNEST_PLACEMENT_STRATEGY_H

#include "../core/Polygon.h"
#include "../core/Point.h"
#include "../core/BoundingBox.h"
#include <vector>
#include <memory>
#include <string>

namespace deepnest {

/**
 * @brief Structure representing a positioned part
 */
struct PlacedPart {
    Polygon polygon;     // The part polygon
    Point position;      // Position (translation)
    double rotation;     // Rotation angle in degrees
    int id;              // Part ID
    int source;          // Source ID

    PlacedPart() : rotation(0.0), id(-1), source(-1) {}
};

/**
 * @brief Abstract base class for placement strategies
 *
 * Different strategies optimize for different objectives when placing parts:
 * - Gravity: Compress parts downward (minimize width*2 + height)
 * - Bounding Box: Minimize rectangular bounding box area
 * - Convex Hull: Minimize convex hull area
 *
 * References:
 * - background.js: placeParts logic (lines 995-1090)
 */
class PlacementStrategy {
public:
    /**
     * @brief Placement strategy types
     */
    enum class Type {
        GRAVITY,        // Compress in gravity direction (weight width more)
        BOUNDING_BOX,   // Minimize bounding box area
        CONVEX_HULL     // Minimize convex hull area
    };

    virtual ~PlacementStrategy() = default;

    /**
     * @brief Find best position for a part
     *
     * Evaluates all candidate positions and returns the one that minimizes
     * the objective function (area, depending on strategy type).
     *
     * @param part Part to be placed
     * @param placed Previously placed parts with positions
     * @param candidatePositions List of valid positions (from NFP)
     * @return Best position, or null Point if no valid position found
     */
    virtual Point findBestPosition(
        const Polygon& part,
        const std::vector<PlacedPart>& placed,
        const std::vector<Point>& candidatePositions
    ) const = 0;

    /**
     * @brief Get strategy type
     */
    virtual Type getType() const = 0;

    /**
     * @brief Get strategy name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Create strategy by type
     */
    static std::unique_ptr<PlacementStrategy> create(Type type);

    /**
     * @brief Create strategy by name
     */
    static std::unique_ptr<PlacementStrategy> create(const std::string& typeName);

protected:
    /**
     * @brief Calculate metric for a given placement
     *
     * Lower values are better. Each strategy implements its own metric.
     *
     * @param part Part being placed
     * @param position Candidate position for part
     * @param placed Previously placed parts
     * @return Metric value (lower is better)
     */
    virtual double calculateMetric(
        const Polygon& part,
        const Point& position,
        const std::vector<PlacedPart>& placed
    ) const = 0;
};

/**
 * @brief Gravity-based placement strategy
 *
 * Minimizes: width*2 + height
 * This weights width more heavily to compress parts in the gravity direction.
 *
 * References:
 * - background.js line 1059-1060: area = rectbounds.width*2 + rectbounds.height
 */
class GravityPlacement : public PlacementStrategy {
public:
    Point findBestPosition(
        const Polygon& part,
        const std::vector<PlacedPart>& placed,
        const std::vector<Point>& candidatePositions
    ) const override;

    Type getType() const override { return Type::GRAVITY; }
    std::string getName() const override { return "gravity"; }

protected:
    double calculateMetric(
        const Polygon& part,
        const Point& position,
        const std::vector<PlacedPart>& placed
    ) const override;
};

/**
 * @brief Bounding box placement strategy
 *
 * Minimizes: width * height (rectangular bounding box area)
 *
 * References:
 * - background.js line 1063: area = rectbounds.width * rectbounds.height
 */
class BoundingBoxPlacement : public PlacementStrategy {
public:
    Point findBestPosition(
        const Polygon& part,
        const std::vector<PlacedPart>& placed,
        const std::vector<Point>& candidatePositions
    ) const override;

    Type getType() const override { return Type::BOUNDING_BOX; }
    std::string getName() const override { return "box"; }

protected:
    double calculateMetric(
        const Polygon& part,
        const Point& position,
        const std::vector<PlacedPart>& placed
    ) const override;
};

/**
 * @brief Convex hull placement strategy
 *
 * Minimizes: convex hull area of all placed parts
 *
 * References:
 * - background.js line 1022, 1067-1075: getHull(allpoints)
 */
class ConvexHullPlacement : public PlacementStrategy {
public:
    Point findBestPosition(
        const Polygon& part,
        const std::vector<PlacedPart>& placed,
        const std::vector<Point>& candidatePositions
    ) const override;

    Type getType() const override { return Type::CONVEX_HULL; }
    std::string getName() const override { return "convexhull"; }

protected:
    double calculateMetric(
        const Polygon& part,
        const Point& position,
        const std::vector<PlacedPart>& placed
    ) const override;
};

} // namespace deepnest

#endif // DEEPNEST_PLACEMENT_STRATEGY_H
