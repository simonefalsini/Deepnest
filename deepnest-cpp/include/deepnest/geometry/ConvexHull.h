#ifndef DEEPNEST_CONVEXHULL_H
#define DEEPNEST_CONVEXHULL_H

#include "../core/Point.h"
#include <vector>

namespace deepnest {

/**
 * @brief Convex Hull calculation using Graham's Scan algorithm
 *
 * This class implements Graham's Scan algorithm for computing
 * the convex hull of a set of 2D points.
 * Based on hull.js from the original DeepNest
 */
class ConvexHull {
public:
    /**
     * @brief Compute convex hull from a set of points
     *
     * Uses Graham's Scan algorithm to find the convex hull.
     * The algorithm runs in O(n log n) time.
     *
     * @param points Input points (at least 3 required)
     * @return Points forming the convex hull in CCW order
     */
    static std::vector<Point> computeHull(const std::vector<Point>& points);

    /**
     * @brief Compute convex hull from a polygon
     *
     * Convenience method that computes the convex hull of a polygon's vertices.
     *
     * @param polygon Input polygon
     * @return Points forming the convex hull in CCW order
     */
    static std::vector<Point> computeHullFromPolygon(const std::vector<Point>& polygon);

private:
    /**
     * @brief Find the anchor point (lowest y, then leftmost x)
     *
     * @param points Input points
     * @param anchorIndex Output parameter for anchor point index
     * @return The anchor point
     */
    static Point findAnchorPoint(const std::vector<Point>& points, size_t& anchorIndex);

    /**
     * @brief Calculate polar angle from anchor to point
     *
     * @param anchor Anchor point
     * @param point Target point
     * @return Angle in degrees
     */
    static double findPolarAngle(const Point& anchor, const Point& point);

    /**
     * @brief Cross product to determine turn direction
     *
     * @param p1 First point
     * @param p2 Second point
     * @param p3 Third point
     * @return Positive if counter-clockwise, negative if clockwise, zero if collinear
     */
    static double crossProduct(const Point& p1, const Point& p2, const Point& p3);

    /**
     * @brief Check if three points make a counter-clockwise turn
     *
     * @param p1 First point
     * @param p2 Second point
     * @param p3 Third point
     * @return True if CCW turn
     */
    static bool isCCW(const Point& p1, const Point& p2, const Point& p3);
};

} // namespace deepnest

#endif // DEEPNEST_CONVEXHULL_H
