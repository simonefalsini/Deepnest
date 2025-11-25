#ifndef DEEPNEST_POINT_H
#define DEEPNEST_POINT_H

#include "Types.h"
#include <QPointF>
#include <cmath>
#include <cstdint>

namespace deepnest {

/**
 * @brief Point structure representing a 2D point with int32 coordinates
 *
 * This structure uses fixed-point integer coordinates for precision and performance.
 * Scale factor: 1000 (1 unit = 0.001mm = 1 micron)
 * 
 * Coordinate range: ±2,147,483 mm (±2147 meters)
 * Precision: 0.001mm (1 micron)
 */
struct Point {
    // Scaled integer coordinates (1 unit = 0.001mm)
    int32_t x;
    int32_t y;
    
    bool exact;   // Used for merge detection - marks if this point is an exact match
    bool marked;  // Used for orbital tracing - tracks visited vertices

    // Scale factor: 1 unit = 0.001mm = 1 micron
    static constexpr int32_t SCALE = 1000;
    static constexpr double INV_SCALE = 1.0 / 1000.0;

    // Constructors
    Point() : x(0), y(0), exact(true), marked(false) {}

    Point(int32_t x_, int32_t y_, bool exact_ = true)
        : x(x_), y(y_), exact(exact_), marked(false) {}

    // Factory methods for conversion from mm (millimeters)
    static Point fromMM(double x_mm, double y_mm, bool exact_ = true) {
        return Point(
            static_cast<int32_t>(std::round(x_mm * SCALE)),
            static_cast<int32_t>(std::round(y_mm * SCALE)),
            exact_
        );
    }

    static Point fromUnits(int32_t x_units, int32_t y_units, bool exact_ = true) {
        return Point(x_units, y_units, exact_);
    }

    // Conversion to mm for display/export
    double xMM() const { return static_cast<double>(x) * INV_SCALE; }
    double yMM() const { return static_cast<double>(y) * INV_SCALE; }

    // Conversion to Boost point
    BoostPoint toBoost() const;

    // Conversion from Boost point (assumes Boost uses mm)
    static Point fromBoost(const BoostPoint& p, bool exact = false);

    // Conversion from Qt point (assumes Qt uses mm)
    static Point fromQt(const QPointF& p, bool exact = false);

    // Conversion to Qt point (returns mm)
    QPointF toQt() const;

    // Vector operations
    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y, exact && other.exact);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y, exact && other.exact);
    }

    Point operator*(int32_t scalar) const {
        return Point(x * scalar, y * scalar, false);  // Scaling loses exactness
    }

    Point operator/(int32_t scalar) const {
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

    Point& operator*=(int32_t scalar) {
        x *= scalar;
        y *= scalar;
        exact = false;  // Scaling loses exactness
        return *this;
    }

    Point& operator/=(int32_t scalar) {
        x /= scalar;
        y /= scalar;
        exact = false;  // Division loses exactness
        return *this;
    }

    // Comparison operators (exact integer comparison)
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }

    // Utility functions

    /**
     * @brief Calculate the distance to another point (in mm)
     * Uses int64 for intermediate calculations to prevent overflow
     */
    double distanceTo(const Point& other) const {
        int64_t dx = static_cast<int64_t>(x) - other.x;
        int64_t dy = static_cast<int64_t>(y) - other.y;
        double distUnits = std::sqrt(static_cast<double>(dx * dx + dy * dy));
        return distUnits * INV_SCALE;  // Return in mm
    }

    /**
     * @brief Calculate the squared distance to another point (in units²)
     * Faster than distanceTo, useful for comparisons
     */
    int64_t distanceSquaredTo(const Point& other) const {
        int64_t dx = static_cast<int64_t>(x) - other.x;
        int64_t dy = static_cast<int64_t>(y) - other.y;
        return dx * dx + dy * dy;
    }

    /**
     * @brief Check if this point is within a certain distance of another point
     * @param distance Distance in mm
     */
    bool withinDistance(const Point& other, double distanceMM) const {
        int64_t distUnits = static_cast<int64_t>(distanceMM * SCALE);
        return distanceSquaredTo(other) < distUnits * distUnits;
    }

    /**
     * @brief Calculate the length (magnitude) of this point as a vector (in mm)
     */
    double length() const {
        int64_t x64 = static_cast<int64_t>(x);
        int64_t y64 = static_cast<int64_t>(y);
        double lenUnits = std::sqrt(static_cast<double>(x64 * x64 + y64 * y64));
        return lenUnits * INV_SCALE;
    }

    /**
     * @brief Calculate the squared length (magnitude) of this point as a vector (in units²)
     */
    int64_t lengthSquared() const {
        int64_t x64 = static_cast<int64_t>(x);
        int64_t y64 = static_cast<int64_t>(y);
        return x64 * x64 + y64 * y64;
    }

    /**
     * @brief Normalize this point as a unit vector
     * Returns a point with length ~1mm (1000 units)
     */
    Point normalize() const {
        double len = length();
        if (len < 0.001) {  // Less than 1 micron
            return Point(0, 0, false);
        }
        double scale = 1.0 / len;  // Scale to 1mm
        return Point::fromMM(xMM() * scale, yMM() * scale, false);
    }

    /**
     * @brief Dot product with another point/vector (in units²)
     * Uses int64 to prevent overflow
     */
    int64_t dot(const Point& other) const {
        int64_t x64 = static_cast<int64_t>(x);
        int64_t y64 = static_cast<int64_t>(y);
        int64_t ox64 = static_cast<int64_t>(other.x);
        int64_t oy64 = static_cast<int64_t>(other.y);
        return x64 * ox64 + y64 * oy64;
    }

    /**
     * @brief Cross product (z-component) with another point/vector (in units²)
     * Uses int64 to prevent overflow
     */
    int64_t cross(const Point& other) const {
        int64_t x64 = static_cast<int64_t>(x);
        int64_t y64 = static_cast<int64_t>(y);
        int64_t ox64 = static_cast<int64_t>(other.x);
        int64_t oy64 = static_cast<int64_t>(other.y);
        return x64 * oy64 - y64 * ox64;
    }

    /**
     * @brief Rotate this point around origin by angle in radians
     * Note: Rotation involves floating point, so result is not exact
     */
    Point rotate(double angleRadians) const {
        double cos_a = std::cos(angleRadians);
        double sin_a = std::sin(angleRadians);
        
        // Convert to mm, rotate, convert back
        double x_mm = xMM();
        double y_mm = yMM();
        
        return Point::fromMM(
            x_mm * cos_a - y_mm * sin_a,
            x_mm * sin_a + y_mm * cos_a,
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
     * @brief Check if two points are almost equal within tolerance
     * @param toleranceMM Tolerance in mm (default 0.001mm = 1 micron)
     */
    static bool almostEqual(const Point& a, const Point& b, double toleranceMM = 0.001) {
        int32_t toleranceUnits = static_cast<int32_t>(toleranceMM * SCALE);
        int32_t dx = std::abs(a.x - b.x);
        int32_t dy = std::abs(a.y - b.y);
        return dx <= toleranceUnits && dy <= toleranceUnits;
    }
};

} // namespace deepnest

#endif // DEEPNEST_POINT_H
