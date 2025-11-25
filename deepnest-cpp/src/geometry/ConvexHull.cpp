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

int ConvexHull::polarCompare(const Point& anchor, const Point& a, const Point& b) {
    // Compare two points by polar angle with respect to anchor using cross product
    // This avoids using atan2 and works with integer arithmetic

    // Create vectors from anchor to a and b
    Point va(a.x - anchor.x, a.y - anchor.y);
    Point vb(b.x - anchor.x, b.y - anchor.y);

    // Use cross product to determine relative angle
    // If cross(va, vb) > 0, then va is counter-clockwise from vb (a comes before b)
    // If cross(va, vb) < 0, then va is clockwise from vb (b comes before a)
    // If cross(va, vb) == 0, they are collinear, sort by distance
    int64_t cross = va.cross(vb);

    if (cross > 0) {
        return -1;  // a comes before b
    } else if (cross < 0) {
        return 1;   // b comes before a
    } else {
        // Collinear: sort by distance from anchor (closer points first)
        int64_t distA = va.dot(va);  // distanceSquared
        int64_t distB = vb.dot(vb);
        if (distA < distB) return -1;
        if (distA > distB) return 1;
        return 0;
    }
}

int64_t ConvexHull::crossProduct(const Point& p1, const Point& p2, const Point& p3) {
    // Cross product of vectors (p1->p2) and (p1->p3)
    // Uses integer arithmetic for precision
    Point v1(p2.x - p1.x, p2.y - p1.y);
    Point v2(p3.x - p1.x, p3.y - p1.y);
    return v1.cross(v2);
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

    // Sort points by polar angle with respect to anchor using cross product
    std::sort(sortedPoints.begin(), sortedPoints.end(),
        [&anchor](const Point& a, const Point& b) {
            return polarCompare(anchor, a, b) < 0;
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
