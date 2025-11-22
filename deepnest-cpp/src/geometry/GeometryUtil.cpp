#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/GeometryUtilAdvanced.h"
#include "../../include/deepnest/geometry/OrbitalTypes.h"
#include "../../include/deepnest/geometry/IntegerNFP.h"
#include "../../include/deepnest/core/Polygon.h"
#include "../../include/deepnest/DebugConfig.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <deque>

namespace deepnest {
namespace GeometryUtil {

// ========== Integer Geometry (for precision) ==========

/**
 * @brief Integer point for exact geometric calculations
 *
 * Using integer coordinates eliminates floating-point precision issues:
 * - slideDistance == 0 is exactly 0 (not ≈ 0)
 * - No accumulation of rounding errors
 * - Same approach as MinkowskiSum
 */
struct IntPoint {
    int64_t x, y;
    bool marked;  // For orbital tracing

    IntPoint() : x(0), y(0), marked(false) {}
    IntPoint(int64_t x_, int64_t y_, bool marked_ = false) : x(x_), y(y_), marked(marked_) {}

    // Convert from double Point
    static IntPoint fromDouble(const Point& p) {
        return IntPoint(static_cast<int64_t>(p.x), static_cast<int64_t>(p.y), p.marked);
    }

    // Convert to double Point
    Point toDouble() const {
        Point p(static_cast<double>(x), static_cast<double>(y));
        p.marked = marked;
        return p;
    }

    // Vector operations
    IntPoint operator+(const IntPoint& other) const {
        return IntPoint(x + other.x, y + other.y);
    }

    IntPoint operator-(const IntPoint& other) const {
        return IntPoint(x - other.x, y - other.y);
    }

    // Dot product
    int64_t dot(const IntPoint& other) const {
        return x * other.x + y * other.y;
    }

    // Cross product (for 2D, returns z-component)
    int64_t cross(const IntPoint& other) const {
        return x * other.y - y * other.x;
    }

    // Squared length (to avoid sqrt)
    int64_t lengthSquared() const {
        return x * x + y * y;
    }
};

// Convert vector of Points to IntPoints
std::vector<IntPoint> toIntPoints(const std::vector<Point>& points) {
    std::vector<IntPoint> result;
    result.reserve(points.size());
    for (const auto& p : points) {
        result.push_back(IntPoint::fromDouble(p));
    }
    return result;
}

// Convert vector of IntPoints to Points
std::vector<Point> toDoublePoints(const std::vector<IntPoint>& points) {
    std::vector<Point> result;
    result.reserve(points.size());
    for (const auto& p : points) {
        result.push_back(p.toDouble());
    }
    return result;
}

// ========== Basic Utility Functions ==========

bool almostEqualPoints(const Point& a, const Point& b, double tolerance) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return (dx * dx + dy * dy) < (tolerance * tolerance);
}

Point normalizeVector(const Point& v) {
    if (almostEqual(v.x * v.x + v.y * v.y, 1.0)) {
        return v;  // Already a unit vector
    }
    return v.normalize();
}

// ========== Line Segment Functions ==========

bool onSegment(const Point& A, const Point& B, const Point& p, double tolerance) {
    // Vertical line
    if (almostEqual(A.x, B.x, tolerance) && almostEqual(p.x, A.x, tolerance)) {
        if (!almostEqual(p.y, B.y, tolerance) && !almostEqual(p.y, A.y, tolerance) &&
            p.y < std::max(B.y, A.y) && p.y > std::min(B.y, A.y)) {
            return true;
        }
        return false;
    }

    // Horizontal line
    if (almostEqual(A.y, B.y, tolerance) && almostEqual(p.y, A.y, tolerance)) {
        if (!almostEqual(p.x, B.x, tolerance) && !almostEqual(p.x, A.x, tolerance) &&
            p.x < std::max(B.x, A.x) && p.x > std::min(B.x, A.x)) {
            return true;
        }
        return false;
    }

    // Range check
    if ((p.x < A.x && p.x < B.x) || (p.x > A.x && p.x > B.x) ||
        (p.y < A.y && p.y < B.y) || (p.y > A.y && p.y > B.y)) {
        return false;
    }

    // Exclude endpoints
    if ((almostEqual(p.x, A.x, tolerance) && almostEqual(p.y, A.y, tolerance)) ||
        (almostEqual(p.x, B.x, tolerance) && almostEqual(p.y, B.y, tolerance))) {
        return false;
    }

    double cross = (p.y - A.y) * (B.x - A.x) - (p.x - A.x) * (B.y - A.y);
    if (std::abs(cross) > tolerance) {
        return false;
    }

    double dot = (p.x - A.x) * (B.x - A.x) + (p.y - A.y) * (B.y - A.y);
    if (dot < 0 || almostEqual(dot, 0, tolerance)) {
        return false;
    }

    double len2 = (B.x - A.x) * (B.x - A.x) + (B.y - A.y) * (B.y - A.y);
    if (dot > len2 || almostEqual(dot, len2, tolerance)) {
        return false;
    }

    return true;
}

std::optional<Point> lineIntersect(const Point& A, const Point& B,
                                   const Point& E, const Point& F,
                                   bool infinite) {
    double a1 = B.y - A.y;
    double b1 = A.x - B.x;
    double c1 = B.x * A.y - A.x * B.y;

    double a2 = F.y - E.y;
    double b2 = E.x - F.x;
    double c2 = F.x * E.y - E.x * F.y;

    double denom = a1 * b2 - a2 * b1;

    double x = (b1 * c2 - b2 * c1) / denom;
    double y = (a2 * c1 - a1 * c2) / denom;

    if (!std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }

    if (!infinite) {
        // Check if intersection point is within both segments
        if (std::abs(A.x - B.x) > TOL) {
            if ((A.x < B.x) ? (x < A.x || x > B.x) : (x > A.x || x < B.x)) {
                return std::nullopt;
            }
        }
        if (std::abs(A.y - B.y) > TOL) {
            if ((A.y < B.y) ? (y < A.y || y > B.y) : (y > A.y || y < B.y)) {
                return std::nullopt;
            }
        }

        if (std::abs(E.x - F.x) > TOL) {
            if ((E.x < F.x) ? (x < E.x || x > F.x) : (x > E.x || x < F.x)) {
                return std::nullopt;
            }
        }
        if (std::abs(E.y - F.y) > TOL) {
            if ((E.y < F.y) ? (y < E.y || y > F.y) : (y > E.y || y < F.y)) {
                return std::nullopt;
            }
        }
    }

    return Point(x, y);
}

// ========== Polygon Functions ==========

BoundingBox getPolygonBounds(const std::vector<Point>& polygon) {
    if (polygon.empty()) {
        return BoundingBox();
    }

    return BoundingBox::fromPoints(polygon);
}

std::optional<bool> pointInPolygon(const Point& point,
                                   const std::vector<Point>& polygon,
                                   double tolerance) {
    if (polygon.size() < 3) {
        return std::nullopt;
    }

    bool inside = false;

    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Point& pi = polygon[i];
        const Point& pj = polygon[j];

        // Check if point is exactly on a vertex
        if (almostEqual(pi.x, point.x, tolerance) &&
            almostEqual(pi.y, point.y, tolerance)) {
            return std::nullopt;
        }

        // Check if point is on a segment
        if (onSegment(pi, pj, point, tolerance)) {
            return std::nullopt;
        }

        // Ignore very small lines
        if (almostEqual(pi.x, pj.x, tolerance) && almostEqual(pi.y, pj.y, tolerance)) {
            continue;
        }

        // Ray casting algorithm
        bool intersect = ((pi.y > point.y) != (pj.y > point.y)) &&
                        (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x);
        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

double polygonArea(const std::vector<Point>& polygon) {
    double area = 0.0;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        area += (polygon[j].x + polygon[i].x) * (polygon[j].y - polygon[i].y);
    }
    return 0.5 * area;
}

bool intersect(const std::vector<Point>& A, const std::vector<Point>& B) {
    // Check for proper segment-segment intersections
    // Excludes "touching" cases where segments share endpoints

    for (size_t i = 0; i < A.size(); i++) {
        size_t nexti = (i == A.size() - 1) ? 0 : i + 1;

        for (size_t j = 0; j < B.size(); j++) {
            size_t nextj = (j == B.size() - 1) ? 0 : j + 1;

            const Point& a1 = A[i];
            const Point& a2 = A[nexti];
            const Point& b1 = B[j];
            const Point& b2 = B[nextj];

            // Check for segment intersection
            auto intersection = lineIntersect(a1, a2, b1, b2, false);
            if (intersection.has_value()) {
                const Point& p = intersection.value();

                // Exclude "touching" cases where intersection is at an endpoint of either segment
                // This allows polygons to touch at vertices without being considered intersecting
                // For NFP, we need to allow:
                // 1. Vertex-to-vertex touching (both endpoints)
                // 2. Vertex-to-edge touching (one endpoint, one interior point)
                bool isA1 = almostEqualPoints(p, a1);
                bool isA2 = almostEqualPoints(p, a2);
                bool isB1 = almostEqualPoints(p, b1);
                bool isB2 = almostEqualPoints(p, b2);

                // If intersection is at an endpoint of EITHER segment, it's just touching
                if (isA1 || isA2 || isB1 || isB2) {
                    continue;  // Skip this touching case
                }

                // This is a proper intersection (crossing) - segments cross in their interiors
                return true;
            }
        }
    }

    return false;
}

bool isRectangle(const std::vector<Point>& poly, double tolerance) {
    BoundingBox bb = getPolygonBounds(poly);

    for (const auto& p : poly) {
        if (!almostEqual(p.x, bb.x, tolerance) &&
            !almostEqual(p.x, bb.x + bb.width, tolerance)) {
            return false;
        }
        if (!almostEqual(p.y, bb.y, tolerance) &&
            !almostEqual(p.y, bb.y + bb.height, tolerance)) {
            return false;
        }
    }

    return true;
}

std::vector<Point> rotatePolygon(const std::vector<Point>& polygon, double angle) {
    std::vector<Point> rotated;
    rotated.reserve(polygon.size());

    double angleRad = degreesToRadians(angle);
    double cosA = std::cos(angleRad);
    double sinA = std::sin(angleRad);

    for (const auto& p : polygon) {
        double x1 = p.x * cosA - p.y * sinA;
        double y1 = p.x * sinA + p.y * cosA;
        rotated.emplace_back(x1, y1);
    }

    return rotated;
}

// ========== Bezier Curve Functions ==========

namespace QuadraticBezier {

bool isFlat(const Point& p1, const Point& p2, const Point& c1, double tol) {
    tol = 4.0 * tol * tol;

    double ux = 2.0 * c1.x - p1.x - p2.x;
    ux *= ux;

    double uy = 2.0 * c1.y - p1.y - p2.y;
    uy *= uy;

    return (ux + uy <= tol);
}

std::pair<BezierSegment, BezierSegment> subdivide(
    const Point& p1, const Point& p2, const Point& c1, double t) {

    Point mid1(
        p1.x + (c1.x - p1.x) * t,
        p1.y + (c1.y - p1.y) * t
    );

    Point mid2(
        c1.x + (p2.x - c1.x) * t,
        c1.y + (p2.y - c1.y) * t
    );

    Point mid3(
        mid1.x + (mid2.x - mid1.x) * t,
        mid1.y + (mid2.y - mid1.y) * t
    );

    BezierSegment seg1{p1, mid3, mid1};
    BezierSegment seg2{mid3, p2, mid2};

    return {seg1, seg2};
}

std::vector<Point> linearize(const Point& p1, const Point& p2, const Point& c1, double tol) {
    std::vector<Point> finished;
    finished.push_back(p1);

    std::deque<BezierSegment> todo;
    todo.push_back({p1, p2, c1});

    while (!todo.empty()) {
        BezierSegment segment = todo.front();

        if (isFlat(segment.p1, segment.p2, segment.c1, tol)) {
            finished.push_back(segment.p2);
            todo.pop_front();
        } else {
            auto divided = subdivide(segment.p1, segment.p2, segment.c1, 0.5);
            todo.pop_front();
            todo.push_front(divided.second);
            todo.push_front(divided.first);
        }
    }

    return finished;
}

} // namespace QuadraticBezier

namespace CubicBezier {

bool isFlat(const Point& p1, const Point& p2, const Point& c1, const Point& c2, double tol) {
    tol = 16.0 * tol * tol;

    double ux = 3.0 * c1.x - 2.0 * p1.x - p2.x;
    ux *= ux;

    double uy = 3.0 * c1.y - 2.0 * p1.y - p2.y;
    uy *= uy;

    double vx = 3.0 * c2.x - 2.0 * p2.x - p1.x;
    vx *= vx;

    double vy = 3.0 * c2.y - 2.0 * p2.y - p1.y;
    vy *= vy;

    if (ux < vx) {
        ux = vx;
    }
    if (uy < vy) {
        uy = vy;
    }

    return (ux + uy <= tol);
}

std::pair<BezierSegment, BezierSegment> subdivide(
    const Point& p1, const Point& p2,
    const Point& c1, const Point& c2, double t) {

    Point mid1(
        p1.x + (c1.x - p1.x) * t,
        p1.y + (c1.y - p1.y) * t
    );

    Point mid2(
        c2.x + (p2.x - c2.x) * t,
        c2.y + (p2.y - c2.y) * t
    );

    Point mid3(
        c1.x + (c2.x - c1.x) * t,
        c1.y + (c2.y - c1.y) * t
    );

    Point mida(
        mid1.x + (mid3.x - mid1.x) * t,
        mid1.y + (mid3.y - mid1.y) * t
    );

    Point midb(
        mid3.x + (mid2.x - mid3.x) * t,
        mid3.y + (mid2.y - mid3.y) * t
    );

    Point midx(
        mida.x + (midb.x - mida.x) * t,
        mida.y + (midb.y - mida.y) * t
    );

    BezierSegment seg1{p1, midx, mid1, mida};
    BezierSegment seg2{midx, p2, midb, mid2};

    return {seg1, seg2};
}

std::vector<Point> linearize(
    const Point& p1, const Point& p2,
    const Point& c1, const Point& c2, double tol) {

    std::vector<Point> finished;
    finished.push_back(p1);

    std::deque<BezierSegment> todo;
    todo.push_back({p1, p2, c1, c2});

    while (!todo.empty()) {
        BezierSegment segment = todo.front();

        if (isFlat(segment.p1, segment.p2, segment.c1, segment.c2, tol)) {
            finished.push_back(segment.p2);
            todo.pop_front();
        } else {
            auto divided = subdivide(segment.p1, segment.p2, segment.c1, segment.c2, 0.5);
            todo.pop_front();
            todo.push_front(divided.second);
            todo.push_front(divided.first);
        }
    }

    return finished;
}

} // namespace CubicBezier

// ========== Arc Functions ==========

namespace Arc {

SvgArc centerToSvg(const Point& center, double rx, double ry,
                   double theta1, double extent, double angleDegrees) {
    double theta2 = theta1 + extent;

    double theta1Rad = degreesToRadians(theta1);
    double theta2Rad = degreesToRadians(theta2);
    double angleRad = degreesToRadians(angleDegrees);

    double cosAngle = std::cos(angleRad);
    double sinAngle = std::sin(angleRad);

    double t1cos = std::cos(theta1Rad);
    double t1sin = std::sin(theta1Rad);
    double t2cos = std::cos(theta2Rad);
    double t2sin = std::sin(theta2Rad);

    double x0 = center.x + cosAngle * rx * t1cos + (-sinAngle) * ry * t1sin;
    double y0 = center.y + sinAngle * rx * t1cos + cosAngle * ry * t1sin;

    double x1 = center.x + cosAngle * rx * t2cos + (-sinAngle) * ry * t2sin;
    double y1 = center.y + sinAngle * rx * t2cos + cosAngle * ry * t2sin;

    int largearc = (extent > 180) ? 1 : 0;
    int sweep = (extent > 0) ? 1 : 0;

    return SvgArc{Point(x0, y0), Point(x1, y1), rx, ry, angleRad, largearc, sweep};
}

CenterArc svgToCenter(const Point& p1, const Point& p2,
                     double rx, double ry, double angleDegrees,
                     int largearc, int sweep) {
    Point mid((p1.x + p2.x) / 2.0, (p1.y + p2.y) / 2.0);
    Point diff((p2.x - p1.x) / 2.0, (p2.y - p1.y) / 2.0);

    double angleRad = degreesToRadians(std::fmod(angleDegrees, 360.0));
    double cosAngle = std::cos(angleRad);
    double sinAngle = std::sin(angleRad);

    double x1 = cosAngle * diff.x + sinAngle * diff.y;
    double y1 = -sinAngle * diff.x + cosAngle * diff.y;

    rx = std::abs(rx);
    ry = std::abs(ry);
    double Prx = rx * rx;
    double Pry = ry * ry;
    double Px1 = x1 * x1;
    double Py1 = y1 * y1;

    double radiiCheck = Px1 / Prx + Py1 / Pry;
    if (radiiCheck > 1.0) {
        double radiiSqrt = std::sqrt(radiiCheck);
        rx = radiiSqrt * rx;
        ry = radiiSqrt * ry;
        Prx = rx * rx;
        Pry = ry * ry;
    }

    int sign = (largearc != sweep) ? -1 : 1;
    double sq = ((Prx * Pry) - (Prx * Py1) - (Pry * Px1)) /
                ((Prx * Py1) + (Pry * Px1));
    sq = (sq < 0) ? 0 : sq;

    double coef = sign * std::sqrt(sq);
    double cx1 = coef * ((rx * y1) / ry);
    double cy1 = coef * -((ry * x1) / rx);

    double cx = mid.x + (cosAngle * cx1 - sinAngle * cy1);
    double cy = mid.y + (sinAngle * cx1 + cosAngle * cy1);

    double ux = (x1 - cx1) / rx;
    double uy = (y1 - cy1) / ry;
    double vx = (-x1 - cx1) / rx;
    double vy = (-y1 - cy1) / ry;
    double n = std::sqrt((ux * ux) + (uy * uy));
    double p = ux;
    sign = (uy < 0) ? -1 : 1;

    double theta = sign * std::acos(p / n);
    theta = radiansToDegrees(theta);

    n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
    p = ux * vx + uy * vy;
    sign = ((ux * vy - uy * vx) < 0) ? -1 : 1;
    double delta = sign * std::acos(p / n);
    delta = radiansToDegrees(delta);

    if (sweep == 1 && delta > 0) {
        delta -= 360.0;
    } else if (sweep == 0 && delta < 0) {
        delta += 360.0;
    }

    delta = std::fmod(delta, 360.0);
    theta = std::fmod(theta, 360.0);

    return CenterArc{Point(cx, cy), rx, ry, theta, delta, angleDegrees};
}

std::vector<Point> linearize(const Point& p1, const Point& p2,
                             double rx, double ry, double angle,
                             int largearc, int sweep, double tol) {
    std::vector<Point> finished;

    CenterArc arc = svgToCenter(p1, p2, rx, ry, angle, largearc, sweep);
    std::deque<CenterArc> todo;
    todo.push_back(arc);

    while (!todo.empty()) {
        arc = todo.front();

        SvgArc fullarc = centerToSvg(arc.center, arc.rx, arc.ry,
                                     arc.theta, arc.extent, arc.angle);
        SvgArc subarc = centerToSvg(arc.center, arc.rx, arc.ry,
                                    arc.theta, 0.5 * arc.extent, arc.angle);
        Point arcmid = subarc.p2;

        Point mid(
            0.5 * (fullarc.p1.x + fullarc.p2.x),
            0.5 * (fullarc.p1.y + fullarc.p2.y)
        );

        if (withinDistance(mid, arcmid, tol)) {
            finished.insert(finished.begin(), fullarc.p2);
            todo.pop_front();
        } else {
            CenterArc arc1{arc.center, arc.rx, arc.ry, arc.theta,
                          0.5 * arc.extent, arc.angle};
            CenterArc arc2{arc.center, arc.rx, arc.ry, arc.theta + 0.5 * arc.extent,
                          0.5 * arc.extent, arc.angle};
            todo.pop_front();
            todo.push_front(arc2);
            todo.push_front(arc1);
        }
    }

    finished.push_back(p2);
    return finished;
}

} // namespace Arc

// Note: Advanced polygon functions (polygonEdge, pointLineDistance, pointDistance,
// segmentDistance, polygonSlideDistance, polygonProjectionDistance, searchStartPoint,
// and polygonHull) are now implemented in GeometryUtilAdvanced.cpp to keep file sizes
// manageable and improve code organization.

// PHASE 3.2: Integer-Based Orbital NFP Implementation
// This provides a fallback when Minkowski sum fails
// Uses exact integer arithmetic to avoid floating-point precision issues
std::vector<std::vector<Point>> noFitPolygon(const std::vector<Point>& A_input,
                                            const std::vector<Point>& B_input,
                                            bool inside,
                                            bool searchEdges) {
    // IMPORTANT: searchEdges parameter is currently IGNORED
    // IntegerNFP::computeNFP always returns a single NFP
    // This is consistent with most use cases where we only need the primary NFP

    LOG_NFP("=== noFitPolygon: Delegating to IntegerNFP::computeNFP ===");
    LOG_NFP("  A size: " << A_input.size() << " points");
    LOG_NFP("  B size: " << B_input.size() << " points");
    LOG_NFP("  Mode: " << (inside ? "INSIDE" : "OUTSIDE"));

    // Delegate to IntegerNFP implementation (exact integer arithmetic)
    // This avoids all floating-point tolerance issues that plagued the old orbital tracing
    auto result = IntegerNFP::computeNFP(A_input, B_input, inside);

    LOG_NFP("=== noFitPolygon: IntegerNFP returned " << result.size() << " NFP(s) ===");

    return result;
}

std::vector<std::vector<Point>> noFitPolygonRectangle(const std::vector<Point>& A,
                                                     const std::vector<Point>& B) {
    // Special case for rectangle NFP
    if (A.size() < 3 || B.size() < 3) {
        return {};
    }

    // Find min/max of A
    double minAx = A[0].x, minAy = A[0].y;
    double maxAx = A[0].x, maxAy = A[0].y;
    for (const auto& p : A) {
        minAx = std::min(minAx, p.x);
        minAy = std::min(minAy, p.y);
        maxAx = std::max(maxAx, p.x);
        maxAy = std::max(maxAy, p.y);
    }

    // Find min/max of B
    double minBx = B[0].x, minBy = B[0].y;
    double maxBx = B[0].x, maxBy = B[0].y;
    for (const auto& p : B) {
        minBx = std::min(minBx, p.x);
        minBy = std::min(minBy, p.y);
        maxBx = std::max(maxBx, p.x);
        maxBy = std::max(maxBy, p.y);
    }

    if (maxBx - minBx > maxAx - minAx || maxBy - minBy > maxAy - minAy) {
        return {};
    }

    std::vector<Point> nfp{
        Point(minAx - minBx + B[0].x, minAy - minBy + B[0].y),
        Point(maxAx - maxBx + B[0].x, minAy - minBy + B[0].y),
        Point(maxAx - maxBx + B[0].x, maxAy - maxBy + B[0].y),
        Point(minAx - minBx + B[0].x, maxAy - maxBy + B[0].y)
    };

    return {nfp};
}

// polygonHull is now implemented in GeometryUtilAdvanced.cpp

// ========== Polygon Simplification ==========

/**
 * Helper function to calculate squared perpendicular distance from a point to a line segment
 */
static double getSquareSegmentDistance(const Point& p, const Point& p1, const Point& p2) {
    double x = p1.x;
    double y = p1.y;
    double dx = p2.x - x;
    double dy = p2.y - y;

    if (dx != 0 || dy != 0) {
        double t = ((p.x - x) * dx + (p.y - y) * dy) / (dx * dx + dy * dy);

        if (t > 1) {
            x = p2.x;
            y = p2.y;
        } else if (t > 0) {
            x += dx * t;
            y += dy * t;
        }
    }

    dx = p.x - x;
    dy = p.y - y;

    return dx * dx + dy * dy;
}

/**
 * Helper function for Douglas-Peucker recursion
 */
static void simplifyDPStep(const std::vector<Point>& points, int first, int last,
                           double sqTolerance, std::vector<Point>& simplified) {
    double maxSqDist = sqTolerance;
    int index = -1;

    for (int i = first + 1; i < last; i++) {
        double sqDist = getSquareSegmentDistance(points[i], points[first], points[last]);

        if (sqDist > maxSqDist) {
            index = i;
            maxSqDist = sqDist;
        }
    }

    if (maxSqDist > sqTolerance) {
        if (index - first > 1) {
            simplifyDPStep(points, first, index, sqTolerance, simplified);
        }
        simplified.push_back(points[index]);
        if (last - index > 1) {
            simplifyDPStep(points, index, last, sqTolerance, simplified);
        }
    }
}

std::vector<Point> simplifyRadialDistance(const std::vector<Point>& points, double sqTolerance) {
    if (points.size() <= 2) {
        return points;
    }

    Point prevPoint = points[0];
    std::vector<Point> newPoints;
    newPoints.push_back(prevPoint);
    Point point;

    for (size_t i = 1; i < points.size(); i++) {
        point = points[i];

        double dx = point.x - prevPoint.x;
        double dy = point.y - prevPoint.y;
        double distSq = dx * dx + dy * dy;

        if (distSq > sqTolerance) {
            newPoints.push_back(point);
            prevPoint = point;
        }
    }

    if (prevPoint.x != point.x || prevPoint.y != point.y) {
        newPoints.push_back(point);
    }

    return newPoints;
}

std::vector<Point> simplifyDouglasPeucker(const std::vector<Point>& points, double sqTolerance) {
    if (points.size() <= 2) {
        return points;
    }

    int last = static_cast<int>(points.size()) - 1;

    std::vector<Point> simplified;
    simplified.push_back(points[0]);
    simplifyDPStep(points, 0, last, sqTolerance, simplified);
    simplified.push_back(points[last]);

    return simplified;
}

std::vector<Point> simplifyPolygon(const std::vector<Point>& points,
                                   double tolerance,
                                   bool highestQuality) {
    if (points.size() <= 2) {
        return points;
    }

    double sqTolerance = tolerance * tolerance;

    std::vector<Point> result = highestQuality ? points : simplifyRadialDistance(points, sqTolerance);
    result = simplifyDouglasPeucker(result, sqTolerance);

    return result;
}

} // namespace GeometryUtil
} // namespace deepnest
