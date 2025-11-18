#ifndef DEEPNEST_BOUNDINGBOX_H
#define DEEPNEST_BOUNDINGBOX_H

#include "Point.h"
#include <algorithm>
#include <limits>

namespace deepnest {

/**
 * @brief Axis-Aligned Bounding Box structure
 *
 * Represents a rectangular bounding box aligned with the coordinate axes.
 * Used for quick overlap tests and spatial queries.
 */
struct BoundingBox {
    double x;      // Left coordinate
    double y;      // Top coordinate
    double width;  // Width of the box
    double height; // Height of the box

    // Constructors
    BoundingBox()
        : x(0.0), y(0.0), width(0.0), height(0.0) {}

    BoundingBox(double x_, double y_, double width_, double height_)
        : x(x_), y(y_), width(width_), height(height_) {}

    // Create from two corner points
    static BoundingBox fromCorners(const Point& p1, const Point& p2) {
        double minX = std::min(p1.x, p2.x);
        double minY = std::min(p1.y, p2.y);
        double maxX = std::max(p1.x, p2.x);
        double maxY = std::max(p1.y, p2.y);
        return BoundingBox(minX, minY, maxX - minX, maxY - minY);
    }

    // Create from a list of points
    static BoundingBox fromPoints(const std::vector<Point>& points) {
        if (points.empty()) {
            return BoundingBox();
        }

        double minX = std::numeric_limits<double>::max();
        double minY = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double maxY = std::numeric_limits<double>::lowest();

        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
        }

        return BoundingBox(minX, minY, maxX - minX, maxY - minY);
    }

    // Getters for corners
    double left() const { return x; }
    double top() const { return y; }
    double right() const { return x + width; }
    double bottom() const { return y + height; }

    Point topLeft() const { return Point(x, y); }
    Point topRight() const { return Point(x + width, y); }
    Point bottomLeft() const { return Point(x, y + height); }
    Point bottomRight() const { return Point(x + width, y + height); }
    Point center() const { return Point(x + width / 2.0, y + height / 2.0); }

    // Area calculation
    double area() const {
        return width * height;
    }

    // Perimeter calculation
    double perimeter() const {
        return 2.0 * (width + height);
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

        double left = std::max(x, other.x);
        double top = std::max(y, other.y);
        double right = std::min(x + width, other.x + other.width);
        double bottom = std::min(y + height, other.y + other.height);

        return BoundingBox(left, top, right - left, bottom - top);
    }

    // Calculate union with another box
    BoundingBox unionWith(const BoundingBox& other) const {
        double left = std::min(x, other.x);
        double top = std::min(y, other.y);
        double right = std::max(x + width, other.x + other.width);
        double bottom = std::max(y + height, other.y + other.height);

        return BoundingBox(left, top, right - left, bottom - top);
    }

    // Expand the box by a margin in all directions
    BoundingBox expand(double margin) const {
        return BoundingBox(
            x - margin,
            y - margin,
            width + 2.0 * margin,
            height + 2.0 * margin
        );
    }

    // Translate the box
    BoundingBox translate(double dx, double dy) const {
        return BoundingBox(x + dx, y + dy, width, height);
    }

    BoundingBox translate(const Point& offset) const {
        return translate(offset.x, offset.y);
    }

    // Scale the box
    BoundingBox scale(double factor) const {
        return BoundingBox(x * factor, y * factor, width * factor, height * factor);
    }

    BoundingBox scale(double sx, double sy) const {
        return BoundingBox(x * sx, y * sy, width * sx, height * sy);
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
