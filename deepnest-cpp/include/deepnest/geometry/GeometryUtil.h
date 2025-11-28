#ifndef DEEPNEST_GEOMETRYUTIL_H
#define DEEPNEST_GEOMETRYUTIL_H

#include "../core/Types.h"
#include "../core/Point.h"
#include "../core/BoundingBox.h"
#include "OrbitalTypes.h"
#include <vector>
#include <optional>

namespace deepnest {

// Forward declaration
class Polygon;

/**
 * @brief Geometry utility functions
 *
 * Collection of static utility functions for geometric operations on points,
 * lines, and polygons. Based on geometryutil.js
 */
namespace GeometryUtil {

    // ========== Basic Utility Functions ==========

    /**
     * @brief Check if two floating point values are almost equal within tolerance
     */
    inline bool almostEqual(double a, double b, double tolerance = TOL) {
        return Point::almostEqual(a, b, tolerance);
    }

    /**
     * @brief Check if two points are almost equal within tolerance
     */
    bool almostEqualPoints(const Point& a, const Point& b, double tolerance = TOL);

    /**
     * @brief Check if two points are within a given distance
     */
    inline bool withinDistance(const Point& p1, const Point& p2, double distance) {
        return p1.withinDistance(p2, distance);
    }

    /**
     * @brief Convert degrees to radians
     */
    inline double degreesToRadians(double angle) {
        return angle * (M_PI / 180.0);
    }

    /**
     * @brief Convert radians to degrees
     */
    inline double radiansToDegrees(double angle) {
        return angle * (180.0 / M_PI);
    }

    /**
     * @brief Normalize a vector to unit length
     */
    Point normalizeVector(const Point& v);

    // ========== Line Segment Functions ==========

    /**
     * @brief Check if point p lies on segment AB (not at endpoints)
     */
    bool onSegment(const Point& A, const Point& B, const Point& p, double tolerance = TOL);

    /**
     * @brief Calculate intersection of line segments AB and EF
     * @param infinite If true, treat as infinite lines instead of segments
     * @return Intersection point, or std::nullopt if no intersection
     */
    std::optional<Point> lineIntersect(
        const Point& A, const Point& B,
        const Point& E, const Point& F,
        bool infinite = false
    );

    // ========== Polygon Functions ==========

    /**
     * @brief Get the bounding box of a polygon
     */
    BoundingBox getPolygonBounds(const std::vector<Point>& polygon);

    /**
     * @brief Check if a point is inside a polygon
     * @return true if inside, false if outside, std::nullopt if exactly on edge/vertex
     */
    std::optional<bool> pointInPolygon(
        const Point& point,
        const std::vector<Point>& polygon,
        double tolerance = TOL
    );

    /**
     * @brief Calculate the signed area of a polygon
     * @return Positive for CCW, negative for CW winding
     */
    double polygonArea(const std::vector<Point>& polygon);

    /**
     * @brief Check if two polygons intersect
     */
    bool intersect(const std::vector<Point>& A, const std::vector<Point>& B);

    /**
     * @brief Check if polygon is a rectangle
     */
    bool isRectangle(const std::vector<Point>& poly, double tolerance = TOL);

    /**
     * @brief Validate polygon for safe usage
     * 
     * Checks for:
     * - Minimum 3 points
     * - No NaN or Inf coordinates
     * - Reasonable coordinate values
     * 
     * @param poly Polygon to validate
     * @param maxDepth Maximum recursion depth for children (default 5)
     * @return true if valid, false otherwise
     */
    bool isValidPolygon(const Polygon& poly, int maxDepth = 5);

    /**
     * @brief Rotate polygon by angle in degrees
     */
    std::vector<Point> rotatePolygon(const std::vector<Point>& polygon, double angle);

    // ========== Bezier Curve Functions ==========

    /**
     * @brief Quadratic Bezier curve utilities
     */
    namespace QuadraticBezier {
        /**
         * @brief Check if quadratic Bezier is flat enough (Roger Willcocks criterion)
         */
        bool isFlat(const Point& p1, const Point& p2, const Point& c1, double tol);

        /**
         * @brief Convert quadratic Bezier to line segments
         */
        std::vector<Point> linearize(const Point& p1, const Point& p2, const Point& c1, double tol);

        /**
         * @brief Subdivide quadratic Bezier at parameter t
         */
        struct BezierSegment {
            Point p1, p2, c1;
        };
        std::pair<BezierSegment, BezierSegment> subdivide(
            const Point& p1, const Point& p2, const Point& c1, double t
        );
    }

    /**
     * @brief Cubic Bezier curve utilities
     */
    namespace CubicBezier {
        /**
         * @brief Check if cubic Bezier is flat enough
         */
        bool isFlat(const Point& p1, const Point& p2, const Point& c1, const Point& c2, double tol);

        /**
         * @brief Convert cubic Bezier to line segments
         */
        std::vector<Point> linearize(
            const Point& p1, const Point& p2,
            const Point& c1, const Point& c2,
            double tol
        );

        /**
         * @brief Subdivide cubic Bezier at parameter t
         */
        struct BezierSegment {
            Point p1, p2, c1, c2;
        };
        std::pair<BezierSegment, BezierSegment> subdivide(
            const Point& p1, const Point& p2,
            const Point& c1, const Point& c2,
            double t
        );
    }

    /**
     * @brief Arc curve utilities
     */
    namespace Arc {
        /**
         * @brief Arc representation (center point format)
         */
        struct CenterArc {
            Point center;
            double rx, ry;
            double theta;   // Start angle in degrees
            double extent;  // Angular extent in degrees
            double angle;   // Rotation angle in degrees
        };

        /**
         * @brief Arc representation (SVG format)
         */
        struct SvgArc {
            Point p1, p2;
            double rx, ry;
            double angle;
            int largearc;
            int sweep;
        };

        /**
         * @brief Convert arc to line segments
         */
        std::vector<Point> linearize(
            const Point& p1, const Point& p2,
            double rx, double ry, double angle,
            int largearc, int sweep, double tol
        );

        /**
         * @brief Convert center arc to SVG arc
         */
        SvgArc centerToSvg(
            const Point& center, double rx, double ry,
            double theta1, double extent, double angleDegrees
        );

        /**
         * @brief Convert SVG arc to center arc
         */
        CenterArc svgToCenter(
            const Point& p1, const Point& p2,
            double rx, double ry, double angleDegrees,
            int largearc, int sweep
        );
    }

    // ========== Advanced Polygon Functions ==========

    /**
     * @brief Get the edge of polygon in the normal direction
     * @param normal Normal vector pointing in desired direction
     * @return Polyline representing the edge
     */
    std::vector<Point> polygonEdge(const std::vector<Point>& polygon, const Point& normal);

    /**
     * @brief Calculate normal distance from point to line segment
     */
    std::optional<double> pointLineDistance(
        const Point& p, const Point& s1, const Point& s2,
        const Point& normal,
        bool s1inclusive = false, bool s2inclusive = false
    );

    /**
     * @brief Calculate signed distance from point to line (for sliding)
     */
    std::optional<double> pointDistance(
        const Point& p, const Point& s1, const Point& s2,
        const Point& normal, bool infinite = false
    );

    /**
     * @brief Calculate minimum distance between two segments in given direction
     */
    std::optional<double> segmentDistance(
        const Point& A, const Point& B,
        const Point& E, const Point& F,
        const Point& direction
    );

    /**
     * @brief Calculate slide distance between two polygons
     */
    std::optional<double> polygonSlideDistance(
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        const Point& direction,
        bool ignoreNegative = false
    );

    /**
     * @brief Project polygon B onto polygon A in given direction
     */
    std::optional<double> polygonProjectionDistance(
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        const Point& direction
    );

    /**
     * @brief Search for a valid start point for NFP calculation
     */
    std::optional<Point> searchStartPoint(
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        bool inside,
        const std::vector<std::vector<Point>>& NFP = {}
    );

    /**
     * @brief Calculate No-Fit Polygon (NFP) of B orbiting around A
     * @param inside If true, B orbits inside A; if false, outside
     * @param searchEdges If true, find all NFPs; if false, only first
     * @return List of NFP polygons
     */
    std::vector<std::vector<Point>> noFitPolygon(
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        bool inside = false,
        bool searchEdges = false
    );

    /**
     * @brief Calculate NFP for special case where A is a rectangle
     */
    std::vector<std::vector<Point>> noFitPolygonRectangle(
        const std::vector<Point>& A,
        const std::vector<Point>& B
    );

    /**
     * @brief Calculate the outer hull of two touching polygons
     * @return Single polygon representing the combined perimeter
     */
    std::optional<std::vector<Point>> polygonHull(
        const std::vector<Point>& A,
        const std::vector<Point>& B
    );

    // ========== Polygon Simplification ==========

    /**
     * @brief Simplify polygon using Ramer-Douglas-Peucker algorithm
     *
     * Based on simplify.js from the original DeepNest JavaScript implementation.
     * Uses a two-pass approach: radial distance filtering followed by
     * Ramer-Douglas-Peucker algorithm for optimal results.
     *
     * @param points Input polygon points
     * @param tolerance Maximum distance a point can deviate from the simplified line
     * @param highestQuality If true, skip radial distance pass for highest quality
     * @return Simplified polygon
     */
    std::vector<Point> simplifyPolygon(
        const std::vector<Point>& points,
        double tolerance = 2.0,
        bool highestQuality = false
    );

    /**
     * @brief Simplify polygon using radial distance
     *
     * First pass: removes points that are too close to their neighbors.
     * This is faster than Douglas-Peucker but less accurate.
     *
     * @param points Input polygon points
     * @param sqTolerance Squared tolerance (distance squared)
     * @return Simplified polygon
     */
    std::vector<Point> simplifyRadialDistance(
        const std::vector<Point>& points,
        double sqTolerance
    );

    /**
     * @brief Simplify polygon using Ramer-Douglas-Peucker algorithm
     *
     * Second pass: uses the Douglas-Peucker algorithm to optimally simplify
     * the polygon while maintaining shape fidelity within tolerance.
     *
     * @param points Input polygon points
     * @param sqTolerance Squared tolerance (distance squared)
     * @return Simplified polygon
     */
    std::vector<Point> simplifyDouglasPeucker(
        const std::vector<Point>& points,
        double sqTolerance
    );

    // ========== Orbital Tracing Helper Functions ==========

    /**
     * @brief Find all touching contacts between polygons A and B
     *
     * Used by orbital tracing algorithm to detect contact points.
     * Implementation in OrbitalHelpers.cpp
     */
    std::vector<TouchingContact> findTouchingContacts(
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        const Point& offsetB
    );

    /**
     * @brief Generate translation vectors from a touching contact
     *
     * Used by orbital tracing algorithm to generate candidate slide directions.
     * Implementation in OrbitalHelpers.cpp
     */
    std::vector<TranslationVector> generateTranslationVectors(
        const TouchingContact& touch,
        const std::vector<Point>& A,
        const std::vector<Point>& B,
        const Point& offsetB
    );

    /**
     * @brief Check if a vector represents backtracking
     *
     * Used by orbital tracing algorithm to filter invalid slide directions.
     * Forward declaration - implementation in OrbitalHelpers.cpp
     */
    bool isBacktracking(
        const TranslationVector& vec,
        const std::optional<TranslationVector>& prevVector
    );

} // namespace GeometryUtil

} // namespace deepnest

#endif // DEEPNEST_GEOMETRYUTIL_H
