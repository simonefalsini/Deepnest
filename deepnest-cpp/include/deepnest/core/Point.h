#ifndef DEEPNEST_POINT_H
#define DEEPNEST_POINT_H

#include "Types.h"
#include <QPointF>
#include <cmath>

namespace deepnest {

/**
 * @brief Point structure representing a 2D point with integer coordinates
 *
 * This structure represents a point in 2D space using integer coordinates (int64_t).
 * All coordinates are in scaled integer space. Conversion to/from floating-point
 * happens only at I/O boundaries using inputScale parameter.
 *
 * IMPORTANT: All internal coordinates are integers. Physical coordinates are
 * converted to integers by multiplying by inputScale (default 10000).
 * Example: 1.5 physical units → 15000 integer units (with scale=10000)
 */
struct Point {
    CoordType x;  // Integer x-coordinate (scaled)
    CoordType y;  // Integer y-coordinate (scaled)
    bool exact;   // Used for merge detection - marks if this point is an exact match
    bool marked;  // Used for orbital tracing - tracks visited vertices

    // Constructors
    Point() : x(0), y(0), exact(true), marked(false) {}  // Default constructor

    Point(CoordType x_, CoordType y_, bool exact_ = true)
        : x(x_), y(y_), exact(exact_), marked(false) {}

    // LINE MERGE FIX: When creating points from explicit coordinates (not curve approximations),
    // they should be marked as exact=true so that MergeDetection can use them.
    // In JavaScript, points are marked exact if they appear in the original polygon.
    // For programmatically generated shapes (rectangles, regular polygons), all points are exact.

    // Conversion to Boost point (already int64_t after Types.h conversion)
    BoostPoint toBoost() const;

    // Conversion from Boost point (already int64_t after Types.h conversion)
    static Point fromBoost(const BoostPoint& p, bool exact = false);

    // Conversion from Qt point (requires scaling)
    // NOTE: This will need inputScale parameter in future (Phase 4)
    static Point fromQt(const QPointF& p, bool exact = false);

    // Conversion to Qt point (requires descaling)
    // NOTE: This will need inputScale parameter in future (Phase 4)
    QPointF toQt() const;

    // Vector operations
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y, exact && other.exact);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y, exact && other.exact);
    }

    // Scalar multiplication (integer)
    Point operator*(CoordType scalar) const {
        return Point(x * scalar, y * scalar, false);  // Scaling loses exactness
    }

    // Scalar division (integer)
    Point operator/(CoordType scalar) const {
        return Point(x / scalar, y / scalar, false);  // Division loses exactness
    }

    // Scalar multiplication with rounding (for transformations)
    Point operator*(double scalar) const {
        return Point(
            static_cast<CoordType>(std::round(x * scalar)),
            static_cast<CoordType>(std::round(y * scalar)),
            false
        );
    }

    // Compound assignment operators
    Point& operator+=(const Point& other) {
        x += other.x;
        y += other.y;
        exact = exact && other.exact;
        return *this;
    }

    Point& operator-=(const Point& other) {
        x -= other.x;
        y -= other.y;
        exact = exact && other.exact;
        return *this;
    }

    Point& operator*=(CoordType scalar) {
        x *= scalar;
        y *= scalar;
        exact = false;  // Scaling loses exactness
        return *this;
    }

    Point& operator*=(double scalar) {
        x = static_cast<CoordType>(std::round(x * scalar));
        y = static_cast<CoordType>(std::round(y * scalar));
        exact = false;  // Scaling loses exactness
        return *this;
    }

    Point& operator/=(CoordType scalar) {
        x /= scalar;
        y /= scalar;
        exact = false;  // Division loses exactness
        return *this;
    }

    // Comparison operators (integer-based)
    bool operator==(const Point& other) const {
        return almostEqual(x, other.x) && almostEqual(y, other.y);
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    // Utility functions

    /**
     * @brief Calculate the distance to another point (returns double for convenience)
     *
     * Note: This converts integer coordinates to double, computes Euclidean distance.
     * For comparisons, prefer distanceSquaredTo() which avoids sqrt and stays in integer domain.
     */
    double distanceTo(const Point& other) const {
        double dx = static_cast<double>(x - other.x);
        double dy = static_cast<double>(y - other.y);
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief Calculate the squared distance to another point (faster, stays in integer domain)
     *
     * WARNING: Overflow risk for very large coordinates!
     * With inputScale=10000, coordinates up to ~3e9 are safe (squared < 2^63).
     * For shapes up to 100k units, this is safe.
     */
    int64_t distanceSquaredTo(const Point& other) const {
        int64_t dx = x - other.x;
        int64_t dy = y - other.y;
        return dx * dx + dy * dy;
    }

    /**
     * @brief Check if this point is within a certain distance of another point
     *
     * @param other The other point
     * @param distance Distance threshold (in integer coordinate space)
     */
    bool withinDistance(const Point& other, CoordType distance) const {
        return distanceSquaredTo(other) < distance * distance;
    }

    /**
     * @brief Calculate the length (magnitude) of this point as a vector (returns double)
     */
    double length() const {
        return std::sqrt(static_cast<double>(lengthSquared()));
    }

    /**
     * @brief Calculate the squared length (magnitude) of this point as a vector
     *
     * WARNING: Overflow risk for very large coordinates!
     * With inputScale=10000, coordinates up to ~3e9 are safe.
     */
    int64_t lengthSquared() const {
        return x * x + y * y;
    }

    /**
     * @brief Dot product with another point/vector (returns int64_t)
     *
     * WARNING: Overflow risk for very large coordinates!
     * Result = x1*x2 + y1*y2. With coords up to ~2e9, products up to ~4e18 are safe.
     */
    int64_t dot(const Point& other) const {
        return x * other.x + y * other.y;
    }

    /**
     * @brief Cross product (z-component) with another point/vector (returns int64_t)
     *
     * WARNING: Overflow risk for very large coordinates!
     * Result = x1*y2 - y1*x2. With coords up to ~2e9, products up to ~4e18 are safe.
     *
     * Used extensively in geometric algorithms for orientation tests.
     */
    int64_t cross(const Point& other) const {
        return x * other.y - y * other.x;
    }

    /**
     * @brief Rotate this point around origin by angle in radians
     *
     * Uses floating-point sin/cos for rotation, then rounds back to integer coordinates.
     * This is acceptable for transformations that happen infrequently (user rotations).
     *
     * For high-precision work, consider using lookup tables or rational approximations.
     */
    Point rotate(double angleRadians) const {
        double cos_a = std::cos(angleRadians);
        double sin_a = std::sin(angleRadians);
        double xd = static_cast<double>(x);
        double yd = static_cast<double>(y);
        return Point(
            static_cast<CoordType>(std::round(xd * cos_a - yd * sin_a)),
            static_cast<CoordType>(std::round(xd * sin_a + yd * cos_a)),
            false
        );
    }

    /**
     * @brief Rotate this point around a center point by angle in radians
     *
     * Uses floating-point sin/cos for rotation, then rounds back to integer coordinates.
     */
    Point rotateAround(const Point& center, double angleRadians) const {
        Point translated = *this - center;
        Point rotated = translated.rotate(angleRadians);
        return rotated + center;
    }

    /**
     * @brief Integer comparison with tolerance
     *
     * For integer coordinates, this simply checks if the absolute difference
     * is within the tolerance. Default tolerance is TOL = 1 (one integer unit).
     */
    static bool almostEqual(CoordType a, CoordType b, CoordType tolerance = TOL) {
        return std::abs(a - b) <= tolerance;
    }
};

} // namespace deepnest

#endif // DEEPNEST_POINT_H
