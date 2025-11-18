#include "../../include/deepnest/geometry/ConvexHull.h"
#include "../../include/deepnest/core/Types.h"
#include <algorithm>
#include <cmath>

namespace deepnest {

Point ConvexHull::findAnchorPoint(const std::vector<Point>& points, size_t& anchorIndex) {
    if (points.empty()) {
        anchorIndex = 0;
        return Point();
    }

    // Find point with lowest y-coordinate, break ties with lowest x-coordinate
    Point anchor = points[0];
    anchorIndex = 0;

    for (size_t i = 1; i < points.size(); i++) {
        const Point& p = points[i];

        // Lower y-coordinate, or same y but lower x
        if (p.y < anchor.y || (Point::almostEqual(p.y, anchor.y) && p.x < anchor.x)) {
            anchor = p;
            anchorIndex = i;
        }
    }

    return anchor;
}

double ConvexHull::findPolarAngle(const Point& anchor, const Point& point) {
    double dx = point.x - anchor.x;
    double dy = point.y - anchor.y;

    if (Point::almostEqual(dx, 0.0) && Point::almostEqual(dy, 0.0)) {
        return 0.0;
    }

    // atan2 returns angle in radians, convert to degrees
    double angle = std::atan2(dy, dx) * 180.0 / M_PI;

    // Normalize to [0, 360)
    if (angle < 0) {
        angle += 360.0;
    }

    return angle;
}

double ConvexHull::crossProduct(const Point& p1, const Point& p2, const Point& p3) {
    // Cross product of vectors (p1->p2) and (p1->p3)
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

bool ConvexHull::isCCW(const Point& p1, const Point& p2, const Point& p3) {
    // Returns true if p1->p2->p3 makes a counter-clockwise turn
    return crossProduct(p1, p2, p3) > 0;
}

std::vector<Point> ConvexHull::computeHull(const std::vector<Point>& points) {
    if (points.size() < 3) {
        return points;
    }

    // Find anchor point (lowest y, then leftmost)
    size_t anchorIndex;
    Point anchor = findAnchorPoint(points, anchorIndex);

    // Create a copy of points excluding the anchor
    std::vector<Point> sortedPoints;
    sortedPoints.reserve(points.size() - 1);
    for (size_t i = 0; i < points.size(); i++) {
        if (i != anchorIndex) {
            sortedPoints.push_back(points[i]);
        }
    }

    // Sort points by polar angle with respect to anchor
    std::sort(sortedPoints.begin(), sortedPoints.end(),
        [&anchor](const Point& a, const Point& b) {
            double angleA = findPolarAngle(anchor, a);
            double angleB = findPolarAngle(anchor, b);

            if (Point::almostEqual(angleA, angleB)) {
                // If angles are equal, sort by distance
                double distA = anchor.distanceSquaredTo(a);
                double distB = anchor.distanceSquaredTo(b);
                return distA < distB;
            }

            return angleA < angleB;
        }
    );

    // If we have less than 2 sorted points, just return anchor + sorted points
    if (sortedPoints.size() < 2) {
        std::vector<Point> hull;
        hull.push_back(anchor);
        hull.insert(hull.end(), sortedPoints.begin(), sortedPoints.end());
        return hull;
    }

    // Graham's Scan
    std::vector<Point> hull;
    hull.push_back(anchor);
    hull.push_back(sortedPoints[0]);
    hull.push_back(sortedPoints[1]);

    for (size_t i = 2; i < sortedPoints.size(); i++) {
        // Remove points that make a clockwise turn
        while (hull.size() >= 2) {
            const Point& p1 = hull[hull.size() - 2];
            const Point& p2 = hull[hull.size() - 1];
            const Point& p3 = sortedPoints[i];

            if (isCCW(p1, p2, p3)) {
                break;  // Good turn, keep it
            }

            hull.pop_back();  // Bad turn, remove middle point
        }

        hull.push_back(sortedPoints[i]);
    }

    return hull;
}

std::vector<Point> ConvexHull::computeHullFromPolygon(const std::vector<Point>& polygon) {
    // Simply delegate to computeHull
    return computeHull(polygon);
}

} // namespace deepnest
