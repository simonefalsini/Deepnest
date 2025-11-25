#ifndef DEEPNEST_POLYGON_H
#define DEEPNEST_POLYGON_H

#include "Types.h"
#include "Point.h"
#include "BoundingBox.h"
#include <QPainterPath>
#include <vector>
#include <memory>

namespace deepnest {

// Forward declarations
namespace GeometryUtil {
    double polygonArea(const std::vector<Point>& polygon);
    BoundingBox getPolygonBounds(const std::vector<Point>& polygon);
}

class Transformation;

/**
 * @brief Polygon class with support for holes and metadata
 *
 * Represents a 2D polygon that can have holes (children).
 * Based on the polygontree structure from deepnest.js
 */
class Polygon {
public:
    // ========== Data Members ==========

    /**
     * @brief Outer boundary points
     */
    std::vector<Point> points;

    /**
     * @brief Holes/children polygons
     */
    std::vector<Polygon> children;

    /**
     * @brief Unique identifier for this polygon
     */
    int id;

    /**
     * @brief Source identifier (e.g., which input file/part)
     */
    int source;

    /**
     * @brief Current rotation angle in degrees
     */
    double rotation;

    /**
     * @brief Quantity/count of this polygon (for nesting)
     */
    int quantity;

    /**
     * @brief Whether this polygon is a sheet/bin
     */
    bool isSheet;

    /**
     * @brief User-defined metadata/name
     */
    std::string name;

    // ========== Constructors ==========

    /**
     * @brief Default constructor
     */
    Polygon();

    /**
     * @brief Construct from points
     */
    explicit Polygon(const std::vector<Point>& pts);

    /**
     * @brief Construct from points with id
     */
    /**
     * @brief Construct from points with id
     */
    Polygon(const std::vector<Point>& pts, int polygonId);

    /**
     * @brief Move constructor
     */
    Polygon(Polygon&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    Polygon& operator=(Polygon&& other) noexcept;

    /**
     * @brief Copy constructor (default is fine, but explicit for clarity alongside move)
     */
    Polygon(const Polygon& other) = default;

    /**
     * @brief Copy assignment operator
     */
    Polygon& operator=(const Polygon& other) = default;

    // ========== Geometric Properties ==========

    /**
     * @brief Calculate area of polygon
     *
     * @return Signed area (positive for CCW, negative for CW)
     */
    double area() const;

    /**
     * @brief Get bounding box
     */
    BoundingBox bounds() const;

    /**
     * @brief Check if polygon is valid (at least 3 points)
     */
    bool isValid() const;

    /**
     * @brief Get number of vertices
     */
    size_t size() const {
        return points.size();
    }

    /**
     * @brief Check if polygon is empty
     */
    bool empty() const {
        return points.empty();
    }

    /**
     * @brief Reverse winding order
     */
    void reverse();

    /**
     * @brief Get reversed copy
     */
    Polygon reversed() const;

    /**
     * @brief Check winding order (CCW = true, CW = false)
     */
    bool isCounterClockwise() const;

    // ========== Transformations ==========

    /**
     * @brief Rotate polygon around origin
     *
     * @param angleDegrees Rotation angle in degrees
     * @return New rotated polygon
     */
    Polygon rotate(double angleDegrees) const;

    /**
     * @brief Rotate polygon around a center point
     *
     * @param angleDegrees Rotation angle in degrees
     * @param center Center of rotation
     * @return New rotated polygon
     */
    Polygon rotateAround(double angleDegrees, const Point& center) const;

    /**
     * @brief Translate polygon
     *
     * @param dx Translation in x
     * @param dy Translation in y
     * @return New translated polygon
     */
    Polygon translate(double dx, double dy) const;

    /**
     * @brief Translate polygon
     *
     * @param offset Translation vector
     * @return New translated polygon
     */
    Polygon translate(const Point& offset) const;

    /**
     * @brief Scale polygon
     *
     * @param factor Uniform scale factor
     * @return New scaled polygon
     */
    Polygon scale(double factor) const;

    /**
     * @brief Scale polygon non-uniformly
     *
     * @param sx Scale factor in x
     * @param sy Scale factor in y
     * @return New scaled polygon
     */
    Polygon scale(double sx, double sy) const;

    /**
     * @brief Apply transformation
     *
     * @param transform Transformation to apply
     * @return New transformed polygon
     */
    Polygon transform(const Transformation& transform) const;

    // ========== Conversions ==========

    /**
     * @brief Convert to Boost polygon
     */
    BoostPolygonWithHoles toBoostPolygon() const;

    /**
     * @brief Convert from Boost polygon
     */
    static Polygon fromBoostPolygon(const BoostPolygonWithHoles& boostPoly);

    /**
     * @brief Convert to QPainterPath (deprecated - use version with scale)
     *
     * WARNING: This doesn't apply proper descaling!
     * Use toQPainterPath(scale) instead.
     */
    QPainterPath toQPainterPath() const;

    /**
     * @brief Convert to QPainterPath with descaling
     *
     * Converts scaled integer coordinates to physical coordinates (double).
     *
     * @param scale Scaling factor (inputScale from DeepNestConfig)
     * @return QPainterPath in physical coordinates
     */
    QPainterPath toQPainterPath(double scale) const;

    /**
     * @brief Convert from QPainterPath (deprecated - use version with scale)
     *
     * WARNING: This doesn't apply proper scaling!
     * Use fromQPainterPath(path, scale) instead.
     */
    static Polygon fromQPainterPath(const QPainterPath& path);

    /**
     * @brief Convert from QPainterPath with scaling
     *
     * Converts physical coordinates (double) to scaled integer coordinates.
     *
     * @param path QPainterPath in physical coordinates
     * @param scale Scaling factor (inputScale from DeepNestConfig)
     * @return Polygon with scaled integer coordinates
     */
    static Polygon fromQPainterPath(const QPainterPath& path, double scale);

    /**
     * @brief Convert from QPainterPath with id (deprecated - use version with scale)
     *
     * WARNING: This doesn't apply proper scaling!
     * Use fromQPainterPath(path, scale, polygonId) instead.
     */
    static Polygon fromQPainterPath(const QPainterPath& path, int polygonId);

    /**
     * @brief Convert from QPainterPath with scaling and id
     *
     * @param path QPainterPath in physical coordinates
     * @param scale Scaling factor (inputScale from DeepNestConfig)
     * @param polygonId Polygon ID
     * @return Polygon with scaled integer coordinates
     */
    static Polygon fromQPainterPath(const QPainterPath& path, double scale, int polygonId);

    /**
     * @brief Extract multiple polygons from a QPainterPath
     *
     * QPainterPath can contain multiple subpaths, this extracts them all
     */
    static std::vector<Polygon> extractFromQPainterPath(const QPainterPath& path);

    // ========== Utilities ==========

    /**
     * @brief Clone this polygon
     */
    Polygon clone() const;

    /**
     * @brief Deep copy with new id
     */
    Polygon copy(int newId) const;

    /**
     * @brief Check if polygon has holes
     */
    bool hasHoles() const {
        return !children.empty();
    }

    /**
     * @brief Get total number of holes
     */
    size_t holeCount() const {
        return children.size();
    }

    /**
     * @brief Add a hole to this polygon
     */
    void addHole(const Polygon& hole);

    /**
     * @brief Clear all holes
     */
    void clearHoles();

    /**
     * @brief Get centroid of polygon
     */
    Point centroid() const;

    /**
     * @brief Offset/expand polygon
     *
     * @param distance Offset distance (positive = expand, negative = contract)
     * @return Vector of resulting polygons
     */
    std::vector<Polygon> offset(double distance) const;

    /**
     * @brief Simplify polygon
     *
     * @param tolerance Simplification tolerance
     * @return Simplified polygon
     */
    Polygon simplify(double tolerance) const;

    // ========== Operators ==========

    /**
     * @brief Access point by index
     */
    Point& operator[](size_t index) {
        return points[index];
    }

    const Point& operator[](size_t index) const {
        return points[index];
    }

private:
    // Helper methods
    void updateMetadataAfterTransform(const Polygon& source);
};

} // namespace deepnest

#endif // DEEPNEST_POLYGON_H
