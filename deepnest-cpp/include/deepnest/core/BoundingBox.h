#ifndef DEEPNEST_BOUNDINGBOX_H
#define DEEPNEST_BOUNDINGBOX_H

#include "Point.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace deepnest {

/**
 * @brief Axis-Aligned Bounding Box structure with integer coordinates
 *
 * Represents a rectangular bounding box aligned with the coordinate axes.
 * All coordinates are in scaled integer space (int64_t).
 * Used for quick overlap tests and spatial queries.
 *
 * IMPORTANT: All coordinates are integers. Physical coordinates are
 * converted to integers by multiplying by inputScale (default 10000).
 */
struct BoundingBox {
    CoordType x;      // Left coordinate (integer)
    CoordType y;      // Top coordinate (integer)
    CoordType width;  // Width of the box (integer)
    CoordType height; // Height of the box (integer)

    // Constructors
    BoundingBox()
        : x(0), y(0), width(0), height(0) {}

    BoundingBox(CoordType x_, CoordType y_, CoordType width_, CoordType height_)
        : x(x_), y(y_), width(width_), height(height_) {}

    // Create from two corner points
    static BoundingBox fromCorners(const Point& p1, const Point& p2) {
        CoordType minX = std::min(p1.x, p2.x);
        CoordType minY = std::min(p1.y, p2.y);
        CoordType maxX = std::max(p1.x, p2.x);
        CoordType maxY = std::max(p1.y, p2.y);
        return BoundingBox(minX, minY, maxX - minX, maxY - minY);
    }

    // Create from a list of points
    static BoundingBox fromPoints(const std::vector<Point>& points) {
        if (points.empty()) {
            return BoundingBox();
        }

        CoordType minX = std::numeric_limits<CoordType>::max();
        CoordType minY = std::numeric_limits<CoordType>::max();
        CoordType maxX = std::numeric_limits<CoordType>::lowest();
        CoordType maxY = std::numeric_limits<CoordType>::lowest();

        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
        }

        return BoundingBox(minX, minY, maxX - minX, maxY - minY);
    }

    // Getters for corners
    CoordType left() const { return x; }
    CoordType top() const { return y; }
    CoordType right() const { return x + width; }
    CoordType bottom() const { return y + height; }

    Point topLeft() const { return Point(x, y); }
    Point topRight() const { return Point(x + width, y); }
    Point bottomLeft() const { return Point(x, y + height); }
    Point bottomRight() const { return Point(x + width, y + height); }
    Point center() const { return Point(x + width / 2, y + height / 2); }

    // Area calculation (returns int64_t, overflow risk for very large boxes)
    int64_t area() const {
        return static_cast<int64_t>(width) * static_cast<int64_t>(height);
    }

    // Perimeter calculation
    int64_t perimeter() const {
        return 2 * (static_cast<int64_t>(width) + static_cast<int64_t>(height));
    }

    // Check if this box contains a point
    bool contains(const Point& p) const {
        return p.x >= x && p.x <= x + width &&
               p.y >= y && p.y <= y + height;
    }

    // Check if this box contains another box
    bool contains(const BoundingBox& other) const {
        return other.x >= x &&
               other.y >= y &&
               other.x + other.width <= x + width &&
               other.y + other.height <= y + height;
    }

    // Check if this box intersects another box
    bool intersects(const BoundingBox& other) const {
        return !(other.x > x + width ||
                 other.x + other.width < x ||
                 other.y > y + height ||
                 other.y + other.height < y);
    }

    // Calculate intersection with another box
    BoundingBox intersection(const BoundingBox& other) const {
        if (!intersects(other)) {
            return BoundingBox();
        }

        CoordType left = std::max(x, other.x);
        CoordType top = std::max(y, other.y);
        CoordType right = std::min(x + width, other.x + other.width);
        CoordType bottom = std::min(y + height, other.y + other.height);

        return BoundingBox(left, top, right - left, bottom - top);
    }

    // Calculate union with another box
    BoundingBox unionWith(const BoundingBox& other) const {
        CoordType left = std::min(x, other.x);
        CoordType top = std::min(y, other.y);
        CoordType right = std::max(x + width, other.x + other.width);
        CoordType bottom = std::max(y + height, other.y + other.height);

        return BoundingBox(left, top, right - left, bottom - top);
    }

    // Expand the box by a margin in all directions
    BoundingBox expand(CoordType margin) const {
        return BoundingBox(
            x - margin,
            y - margin,
            width + 2 * margin,
            height + 2 * margin
        );
    }

    // Translate the box
    BoundingBox translate(CoordType dx, CoordType dy) const {
        return BoundingBox(x + dx, y + dy, width, height);
    }

    BoundingBox translate(const Point& offset) const {
        return translate(offset.x, offset.y);
    }

    // Scale the box (integer factor)
    BoundingBox scale(CoordType factor) const {
        return BoundingBox(x * factor, y * factor, width * factor, height * factor);
    }

    BoundingBox scale(CoordType sx, CoordType sy) const {
        return BoundingBox(x * sx, y * sy, width * sx, height * sy);
    }

    // Scale the box (double factor with rounding, for transformations)
    BoundingBox scale(double factor) const {
        return BoundingBox(
            static_cast<CoordType>(std::round(x * factor)),
            static_cast<CoordType>(std::round(y * factor)),
            static_cast<CoordType>(std::round(width * factor)),
            static_cast<CoordType>(std::round(height * factor))
        );
    }

    BoundingBox scale(double sx, double sy) const {
        return BoundingBox(
            static_cast<CoordType>(std::round(x * sx)),
            static_cast<CoordType>(std::round(y * sy)),
            static_cast<CoordType>(std::round(width * sx)),
            static_cast<CoordType>(std::round(height * sy))
        );
    }

    // Check if the box is valid (positive dimensions)
    bool isValid() const {
        return width > 0 && height > 0;
    }

    // Comparison operators
    bool operator==(const BoundingBox& other) const {
        return Point::almostEqual(x, other.x) &&
               Point::almostEqual(y, other.y) &&
               Point::almostEqual(width, other.width) &&
               Point::almostEqual(height, other.height);
    }

    bool operator!=(const BoundingBox& other) const {
        return !(*this == other);
    }
};

} // namespace deepnest

#endif // DEEPNEST_BOUNDINGBOX_H
