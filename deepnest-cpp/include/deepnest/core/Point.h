#ifndef DEEPNEST_POINT_H
#define DEEPNEST_POINT_H

#include "Types.h"
#include <QPointF>
#include <cmath>

namespace deepnest {

/**
 * @brief Point structure representing a 2D point
 *
 * This structure represents a point in 2D space and provides conversions
 * between different representations (Boost, Qt).
 */
struct Point {
    double x;
    double y;
    bool exact;   // Used for merge detection - marks if this point is an exact match
    bool marked;  // Used for orbital tracing - tracks visited vertices

    // Constructors
    Point() : x(0.0), y(0.0), exact(true), marked(false) {}  // Default constructor

    Point(double x_, double y_, bool exact_ = true)
        : x(x_), y(y_), exact(exact_), marked(false) {}

    // LINE MERGE FIX: When creating points from explicit coordinates (not curve approximations),
    // they should be marked as exact=true so that MergeDetection can use them.
    // In JavaScript, points are marked exact if they appear in the original polygon.
    // For programmatically generated shapes (rectangles, regular polygons), all points are exact.

    // Conversion to Boost point
    BoostPoint toBoost() const;

    // Conversion from Boost point
    static Point fromBoost(const BoostPoint& p, bool exact = false);

    // Conversion from Qt point
    static Point fromQt(const QPointF& p, bool exact = false);

    // Conversion to Qt point
    QPointF toQt() const;

    // Vector operations
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y, exact && other.exact);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y, exact && other.exact);
    }

    Point operator*(double scalar) const {
        return Point(x * scalar, y * scalar, false);  // Scaling loses exactness
    }

    Point operator/(double scalar) const {
        return Point(x / scalar, y / scalar, false);  // Division loses exactness
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

    Point& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        exact = false;  // Scaling loses exactness
        return *this;
    }

    Point& operator/=(double scalar) {
        x /= scalar;
        y /= scalar;
        exact = false;  // Division loses exactness
        return *this;
    }

    // Comparison operators
    bool operator==(const Point& other) const {
        return almostEqual(x, other.x) && almostEqual(y, other.y);
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    // Utility functions

    /**
     * @brief Calculate the distance to another point
     */
    double distanceTo(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief Calculate the squared distance to another point (faster than distanceTo)
     */
    double distanceSquaredTo(const Point& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return dx * dx + dy * dy;
    }

    /**
     * @brief Check if this point is within a certain distance of another point
     */
    bool withinDistance(const Point& other, double distance) const {
        return distanceSquaredTo(other) < distance * distance;
    }

    /**
     * @brief Calculate the length (magnitude) of this point as a vector
     */
    double length() const {
        return std::sqrt(x * x + y * y);
    }

    /**
     * @brief Calculate the squared length (magnitude) of this point as a vector
     */
    double lengthSquared() const {
        return x * x + y * y;
    }

    /**
     * @brief Normalize this point as a unit vector
     */
    Point normalize() const {
        double len = length();
        if (almostEqual(len, 0.0)) {
            return Point(0, 0, false);
        }
        double inverse = 1.0 / len;
        return Point(x * inverse, y * inverse, false);
    }

    /**
     * @brief Dot product with another point/vector
     */
    double dot(const Point& other) const {
        return x * other.x + y * other.y;
    }

    /**
     * @brief Cross product (z-component) with another point/vector
     */
    double cross(const Point& other) const {
        return x * other.y - y * other.x;
    }

    /**
     * @brief Rotate this point around origin by angle in radians
     */
    Point rotate(double angleRadians) const {
        double cos_a = std::cos(angleRadians);
        double sin_a = std::sin(angleRadians);
        return Point(
            x * cos_a - y * sin_a,
            x * sin_a + y * cos_a,
            false
        );
    }

    /**
     * @brief Rotate this point around a center point by angle in radians
     */
    Point rotateAround(const Point& center, double angleRadians) const {
        Point translated = *this - center;
        Point rotated = translated.rotate(angleRadians);
        return rotated + center;
    }

    /**
     * @brief Floating point comparison with tolerance
     */
    static bool almostEqual(double a, double b, double tolerance = TOL) {
        return std::abs(a - b) < tolerance;
    }
};

} // namespace deepnest

#endif // DEEPNEST_POINT_H
