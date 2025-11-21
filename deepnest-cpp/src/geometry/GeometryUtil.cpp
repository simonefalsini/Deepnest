#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/GeometryUtilAdvanced.h"
#include "../../include/deepnest/geometry/OrbitalTypes.h"
#include "../../include/deepnest/core/Polygon.h"
#include "../../include/deepnest/DebugConfig.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <deque>

namespace deepnest {
namespace GeometryUtil {

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
    // This is a simplified version - the full implementation is quite complex
    // For now, we check segment-segment intersections

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

// PHASE 3.2: Complete Orbital-Based noFitPolygon implementation
// This provides a fallback when Minkowski sum fails or for validation
// Reference: geometryutil.js:1437-1727 (noFitPolygon function)
std::vector<std::vector<Point>> noFitPolygon(const std::vector<Point>& A_input,
                                            const std::vector<Point>& B_input,
                                            bool inside,
                                            bool searchEdges) {
    // Complete rewrite based on JavaScript geometryutil.js lines 1437-1727
    // This implementation follows the JavaScript algorithm exactly

    LOG_NFP("=== ORBITAL TRACING START ===");
    LOG_NFP("  A size: " << A_input.size() << " points");
    LOG_NFP("  B size: " << B_input.size() << " points");
    LOG_NFP("  Mode: " << (inside ? "INSIDE" : "OUTSIDE"));

    if (A_input.size() < 3 || B_input.size() < 3) {
        LOG_NFP("  ERROR: Polygon has < 3 points");
        return {};
    }

    // Make working copies that we can modify
    std::vector<Point> A = A_input;
    std::vector<Point> B = B_input;

    // CRITICAL: Ensure correct winding order for orbital tracing
    // OUTSIDE NFP requires both polygons to be CCW
    // INSIDE NFP requires A to be CW, B to be CCW
    double areaA = polygonArea(A);
    double areaB = polygonArea(B);

    if (!inside) {
        // OUTSIDE: Both must be CCW (positive area)
        if (areaA < 0) {
            LOG_NFP("  Correcting A orientation: CW → CCW");
            std::reverse(A.begin(), A.end());
        }
        if (areaB < 0) {
            LOG_NFP("  Correcting B orientation: CW → CCW");
            std::reverse(B.begin(), B.end());
        }
    } else {
        // INSIDE: A must be CW (negative area), B must be CCW (positive area)
        if (areaA > 0) {
            LOG_NFP("  Correcting A orientation: CCW → CW");
            std::reverse(A.begin(), A.end());
        }
        if (areaB < 0) {
            LOG_NFP("  Correcting B orientation: CW → CCW");
            std::reverse(B.begin(), B.end());
        }
    }

    // Initialize marked flags (used to track visited vertices)
    for (auto& p : A) p.marked = false;
    for (auto& p : B) p.marked = false;

    std::vector<std::vector<Point>> nfpList;

    // Get initial start point
    std::optional<Point> startOpt;

    if (!inside) {
        // OUTSIDE NFP: Use heuristic - place bottom of A at top of B
        // JavaScript lines 1447-1475
        size_t minAindex = 0;
        double minAy = A[0].y;
        for (size_t i = 1; i < A.size(); i++) {
            if (A[i].y < minAy) {
                minAy = A[i].y;
                minAindex = i;
            }
        }

        size_t maxBindex = 0;
        double maxBy = B[0].y;
        for (size_t i = 1; i < B.size(); i++) {
            if (B[i].y > maxBy) {
                maxBy = B[i].y;
                maxBindex = i;
            }
        }

        startOpt = Point(
            A[minAindex].x - B[maxBindex].x,
            A[minAindex].y - B[maxBindex].y
        );
        LOG_NFP("  Start point (heuristic): (" << startOpt->x << ", " << startOpt->y << ")");
    }
    else {
        // INSIDE NFP: No reliable heuristic, use search
        // JavaScript lines 1477-1479
        startOpt = searchStartPoint(A, B, true, {});
        if (startOpt.has_value()) {
            LOG_NFP("  Start point (search): (" << startOpt->x << ", " << startOpt->y << ")");
        } else {
            LOG_NFP("  ERROR: No start point found!");
        }
    }

    // Main loop: find all NFPs starting from different points
    // JavaScript lines 1483-1724
    while (startOpt.has_value()) {
        LOG_NFP("  --- New NFP loop iteration ---");
        Point offsetB = startOpt.value();

        std::vector<Point> nfp;
        std::optional<TranslationVector> prevVector;  // Changed from pointer to value

        // Reference point: B[0] translated by offset
        // JavaScript lines 1497-1500
        Point reference(B[0].x + offsetB.x, B[0].y + offsetB.y);
        Point startPoint = reference;

        nfp.push_back(reference);

        int counter = 0;
        int maxIterations = 10 * (A.size() + B.size());

        // Main orbital tracing loop
        // JavaScript lines 1503-1711
        while (counter < maxIterations) {
            // STEP 1: Find all touching contacts
            // JavaScript lines 1504-1520
            auto touchingList = findTouchingContacts(A, B, offsetB);

            LOG_NFP("    Iteration " << counter << ": touching contacts = " << touchingList.size());

            if (touchingList.empty()) {
                LOG_NFP("    ERROR: No touching contacts found, breaking loop");
                break;  // No touching contacts
            }

            // STEP 2: Generate translation vectors from all touches
            // JavaScript lines 1522-1616
            std::vector<TranslationVector> allVectors;
            for (const auto& touch : touchingList) {
                // Mark vertex as touched
                A[touch.indexA].marked = true;

                LOG_NFP("    Touch type=" << (int)touch.type << " A[" << touch.indexA << "] B[" << touch.indexB << "]");

                auto vectors = generateTranslationVectors(touch, A, B, offsetB);

                for (const auto& v : vectors) {
                    LOG_NFP("      Vector: (" << v.x << ", " << v.y << ") len=" << v.length()
                           << " polygon=" << v.polygon);
                }

                allVectors.insert(allVectors.end(), vectors.begin(), vectors.end());
            }

            LOG_NFP("    Generated " << allVectors.size() << " translation vectors");
            if (prevVector.has_value()) {
                LOG_NFP("    prevVector: (" << prevVector->x << ", " << prevVector->y
                       << ") polygon=" << prevVector->polygon);
            }

            // STEP 3: Filter and select best vector
            // JavaScript lines 1620-1657
            TranslationVector* bestVector = nullptr;
            double maxDistance = 0.0;
            int filteredCount = 0;

            for (auto& vec : allVectors) {
                // Skip zero vectors and backtracking
                // JavaScript lines 1624-1642
                if (isBacktracking(vec, prevVector)) {
                    LOG_NFP("      FILTERED backtrack: (" << vec.x << ", " << vec.y << ")");
                    filteredCount++;
                    continue;
                }

                // Calculate slide distance
                // JavaScript line 1645
                auto slideOpt = polygonSlideDistance(A, B, Point(vec.x, vec.y), true);

                double slideDistance;
                double vecLength2 = vec.x * vec.x + vec.y * vec.y;

                // JavaScript lines 1648-1651: if null or too large, use vector length
                if (!slideOpt.has_value() || slideOpt.value() * slideOpt.value() > vecLength2) {
                    slideDistance = std::sqrt(vecLength2);
                }
                else {
                    slideDistance = slideOpt.value();
                }

                LOG_NFP("      Candidate: (" << vec.x << ", " << vec.y << ") slide=" << slideDistance
                       << " polygon=" << vec.polygon);

                // Select vector with MAXIMUM distance
                // JavaScript lines 1653-1656
                if (slideDistance > maxDistance) {
                    maxDistance = slideDistance;
                    bestVector = &vec;
                }
            }

            LOG_NFP("    Filtered " << filteredCount << " backtracking vectors");
            LOG_NFP("    Best vector: (" << (bestVector ? bestVector->x : 0) << ", "
                   << (bestVector ? bestVector->y : 0) << ") maxDistance = " << maxDistance
                   << " polygon=" << (bestVector ? std::string(1, bestVector->polygon) : "?"));

            // JavaScript lines 1660-1664: check if valid vector found
            if (!bestVector || almostEqual(maxDistance, 0.0)) {
                LOG_NFP("    ERROR: No valid vector found (bestVector=" << (bestVector ? "exists" : "null")
                       << ", maxDistance=" << maxDistance << ")");
                nfp.clear();  // Didn't close loop properly
                break;
            }

            // Mark vertices as used
            // JavaScript lines 1666-1667
            if (bestVector->polygon == 'A') {
                A[bestVector->startIndex].marked = true;
                A[bestVector->endIndex].marked = true;
            } else {
                B[bestVector->startIndex].marked = true;
                B[bestVector->endIndex].marked = true;
            }

            // STEP 4: Trim vector if needed
            // JavaScript lines 1672-1677
            double vecLength2 = bestVector->x * bestVector->x + bestVector->y * bestVector->y;
            if (maxDistance * maxDistance < vecLength2 &&
                !almostEqual(maxDistance * maxDistance, vecLength2)) {
                double scale = std::sqrt((maxDistance * maxDistance) / vecLength2);
                bestVector->x *= scale;
                bestVector->y *= scale;
            }

            // Save prevVector AFTER trimming (so it matches the actual movement)
            prevVector = *bestVector;

            // STEP 5: Move reference point
            // JavaScript lines 1679-1680
            reference.x += bestVector->x;
            reference.y += bestVector->y;
            LOG_NFP("    New reference: (" << reference.x << ", " << reference.y << ")");

            // STEP 6: Check loop closure
            // JavaScript lines 1682-1700
            if (almostEqual(reference.x, startPoint.x) && almostEqual(reference.y, startPoint.y)) {
                LOG_NFP("    Loop closed: returned to start point (" << startPoint.x << ", " << startPoint.y << ")");
                LOG_NFP("    Distance from start: " << std::sqrt((reference.x - startPoint.x)*(reference.x - startPoint.x) +
                       (reference.y - startPoint.y)*(reference.y - startPoint.y)));
                break;  // Completed loop
            }

            // Check if we've returned to any previous point
            // JavaScript lines 1688-1700
            bool looped = false;
            if (nfp.size() > 0) {
                for (size_t i = 0; i < nfp.size() - 1; i++) {
                    if (almostEqual(reference.x, nfp[i].x) && almostEqual(reference.y, nfp[i].y)) {
                        LOG_NFP("    Loop closed: returned to point " << i << " ("
                               << nfp[i].x << ", " << nfp[i].y << ")");
                        looped = true;
                        break;
                    }
                }
            }

            if (looped) {
                break;  // Completed loop
            }

            // Add point to NFP
            // JavaScript lines 1702-1705
            nfp.push_back(reference);

            // Update offset for next iteration
            // JavaScript lines 1707-1708
            offsetB.x += bestVector->x;
            offsetB.y += bestVector->y;

            counter++;
        }

        // Add NFP if valid
        // JavaScript lines 1713-1715
        if (!nfp.empty() && nfp.size() >= 3) {
            LOG_NFP("  ✓ Generated NFP with " << nfp.size() << " points");
            nfpList.push_back(nfp);
        } else {
            LOG_NFP("  ✗ NFP invalid: size = " << nfp.size());
        }

        if (!searchEdges) {
            break;  // Only get first NFP
        }

        // Search for next start point
        // JavaScript line 1722
        startOpt = searchStartPoint(A, B, inside, nfpList);
    }

    LOG_NFP("=== ORBITAL TRACING COMPLETE: " << nfpList.size() << " NFPs generated ===");

    // JavaScript line 1726
    return nfpList;
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
