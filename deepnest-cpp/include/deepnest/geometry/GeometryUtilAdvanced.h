#ifndef DEEPNEST_GEOMETRY_UTIL_ADVANCED_H
#define DEEPNEST_GEOMETRY_UTIL_ADVANCED_H

/**
 * @file GeometryUtilAdvanced.h
 * @brief Advanced geometric utility functions for NFP calculation
 *
 * This file contains the more complex geometric algorithms that are used
 * primarily for No-Fit Polygon (NFP) calculation. These functions were
 * separated from GeometryUtil.h to keep file sizes manageable and improve
 * code organization.
 *
 * These functions are direct ports from the JavaScript geometryutil.js
 */

#include "../core/Types.h"
#include "../core/Point.h"
#include <vector>
#include <optional>

namespace deepnest {
namespace GeometryUtil {

/**
 * @brief Extract the edge of a polygon facing a given direction
 *
 * Given a polygon and a normal vector, this function finds the edge (sequence
 * of vertices) that faces the direction perpendicular to that normal.
 * This is used in NFP calculation to find collision edges.
 *
 * @param polygon Input polygon vertices
 * @param normal Normal vector indicating the direction
 * @return Sequence of points forming the edge, or empty if invalid
 */
std::vector<Point> polygonEdge(const std::vector<Point>& polygon, const Point& normal);

/**
 * @brief Calculate normal distance from point to line segment
 *
 * Returns the signed distance from point p to the line segment (s1, s2)
 * projected onto the normal vector. Used for collision detection.
 *
 * @param p Point to test
 * @param s1 First endpoint of line segment
 * @param s2 Second endpoint of line segment
 * @param normal Direction vector for distance measurement
 * @param s1inclusive If true, include s1 endpoint
 * @param s2inclusive If true, include s2 endpoint
 * @return Signed distance, or nullopt if point doesn't project onto segment
 */
std::optional<double> pointLineDistance(
    const Point& p,
    const Point& s1,
    const Point& s2,
    const Point& normal,
    bool s1inclusive,
    bool s2inclusive
);

/**
 * @brief Calculate signed distance from point to line (for sliding)
 *
 * Similar to pointLineDistance but used for calculating slide distances
 * when moving polygons. The sign indicates the direction of movement needed.
 *
 * @param p Point to test
 * @param s1 First endpoint of line
 * @param s2 Second endpoint of line
 * @param normal Direction vector for distance measurement
 * @param infinite If true, treat line as infinite (not just a segment)
 * @return Signed distance, or nullopt if no collision
 */
std::optional<double> pointDistance(
    const Point& p,
    const Point& s1,
    const Point& s2,
    const Point& normal,
    bool infinite
);

/**
 * @brief Calculate minimum distance between two line segments in given direction
 *
 * Finds the minimum distance that segment AB needs to move in the given
 * direction before it collides with segment EF. Used for polygon sliding.
 *
 * @param A First endpoint of first segment
 * @param B Second endpoint of first segment
 * @param E First endpoint of second segment
 * @param F Second endpoint of second segment
 * @param direction Direction vector for movement
 * @return Distance until collision, or nullopt if no collision
 */
std::optional<double> segmentDistance(
    const Point& A,
    const Point& B,
    const Point& E,
    const Point& F,
    const Point& direction
);

/**
 * @brief Calculate slide distance between two polygons
 *
 * Determines how far polygon A can slide in the given direction before
 * colliding with polygon B. This is a key function for NFP calculation.
 *
 * @param A First polygon
 * @param B Second polygon
 * @param direction Direction vector for sliding
 * @param ignoreNegative If true, ignore negative (backwards) distances
 * @return Slide distance, or nullopt if no collision
 */
std::optional<double> polygonSlideDistance(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& direction,
    bool ignoreNegative
);

/**
 * @brief Project polygon B onto polygon A in given direction
 *
 * Projects each vertex of B onto A's edges and finds the maximum
 * projection distance. Used for finding touch points during NFP calculation.
 *
 * @param A Target polygon
 * @param B Polygon to project
 * @param direction Projection direction
 * @return Maximum projection distance, or nullopt if no projection
 */
std::optional<double> polygonProjectionDistance(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& direction
);

/**
 * @brief Search for a valid start point for NFP calculation
 *
 * Finds a position where polygon B can be placed relative to polygon A
 * without overlap (or with overlap if inside=true). This is the starting
 * point for tracing the NFP.
 *
 * @param A Fixed polygon
 * @param B Moving polygon
 * @param inside If true, find interior NFP; if false, find exterior NFP
 * @param NFP Previously calculated NFP points to avoid (optional)
 * @return Start point offset for B, or nullopt if none found
 */
std::optional<Point> searchStartPoint(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    bool inside,
    const std::vector<std::vector<Point>>& NFP
);

/**
 * @brief Calculate the convex hull of two positioned polygons
 *
 * Given two polygons A and B (with optional offsets), computes the combined
 * convex hull representing their outer perimeter. Used for bounding calculations.
 *
 * The function assumes polygons may have offset coordinates (offsetx, offsety)
 * which are handled internally.
 *
 * @param A First polygon
 * @param B Second polygon
 * @return Combined hull polygon, or nullopt if invalid input
 */
std::optional<std::vector<Point>> polygonHull(
    const std::vector<Point>& A,
    const std::vector<Point>& B
);

} // namespace GeometryUtil
} // namespace deepnest

#endif // DEEPNEST_GEOMETRY_UTIL_ADVANCED_H
