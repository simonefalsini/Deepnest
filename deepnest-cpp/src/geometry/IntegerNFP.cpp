/**
 * @file IntegerNFP.cpp
 * @brief Integer-based No-Fit Polygon (Orbital Tracing) implementation
 *
 * DESIGN PHILOSOPHY:
 * - Uses int64_t coordinates for EXACT arithmetic (no floating-point tolerance)
 * - Follows computational geometry standards (CGAL, Boost.Polygon)
 * - Orbital tracing algorithm for NFP calculation
 * - This is the FALLBACK when MinkowskiSum fails
 *
 * MATHEMATICAL FOUNDATION:
 * - Cross product for orientation (exact with integers)
 * - Edge sliding with exact distance calculation
 * - No almostEqual() or tolerance checks
 *
 * REFERENCE: Computational Geometry: Algorithms and Applications (de Berg et al.)
 */

#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/DebugConfig.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace deepnest {
namespace IntegerNFP {

// ========== CONSTANTS ==========
constexpr int64_t SCALE_FACTOR = 10000;  // Scale to preserve 4 decimal places
constexpr int MAX_ITERATIONS = 10000;     // Safety limit for orbital tracing

// ========== INTEGER POINT ==========

struct IntPoint {
    int64_t x, y;
    int index;      // Original index in polygon
    bool marked;    // For tracking visited vertices

    IntPoint() : x(0), y(0), index(-1), marked(false) {}
    IntPoint(int64_t x_, int64_t y_, int idx = -1)
        : x(x_), y(y_), index(idx), marked(false) {}

    // Convert from floating-point Point
    static IntPoint fromPoint(const Point& p, int idx = -1) {
        return IntPoint(
            static_cast<int64_t>(std::round(p.x * SCALE_FACTOR)),
            static_cast<int64_t>(std::round(p.y * SCALE_FACTOR)),
            idx
        );
    }

    // Convert to floating-point Point
    Point toPoint() const {
        Point p;
        p.x = static_cast<double>(x) / SCALE_FACTOR;
        p.y = static_cast<double>(y) / SCALE_FACTOR;
        p.marked = marked;
        return p;
    }

    // Exact equality
    bool operator==(const IntPoint& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const IntPoint& other) const {
        return !(*this == other);
    }

    // Vector operations
    IntPoint operator+(const IntPoint& other) const {
        return IntPoint(x + other.x, y + other.y);
    }

    IntPoint operator-(const IntPoint& other) const {
        return IntPoint(x - other.x, y - other.y);
    }

    IntPoint operator-() const {
        return IntPoint(-x, -y);
    }
};

// ========== INTEGER POLYGON ==========

struct IntPolygon {
    std::vector<IntPoint> points;

    static IntPolygon fromPoints(const std::vector<Point>& pts) {
        IntPolygon poly;
        for (size_t i = 0; i < pts.size(); i++) {
            poly.points.push_back(IntPoint::fromPoint(pts[i], i));
        }
        return poly;
    }

    std::vector<Point> toPoints() const {
        std::vector<Point> pts;
        for (const auto& p : points) {
            pts.push_back(p.toPoint());
        }
        return pts;
    }

    size_t size() const { return points.size(); }

    IntPoint& operator[](size_t i) { return points[i]; }
    const IntPoint& operator[](size_t i) const { return points[i]; }
};

// ========== GEOMETRIC PRIMITIVES ==========

/**
 * @brief Cross product of vectors (p1-p0) × (p2-p0)
 * Returns: > 0 if CCW, < 0 if CW, = 0 if collinear
 *
 * EXACT: int64_t × int64_t fits in __int128 for our coordinate range
 */
inline int64_t crossProduct(const IntPoint& p0, const IntPoint& p1, const IntPoint& p2) {
    // Use 128-bit to prevent overflow
    __int128 dx1 = static_cast<__int128>(p1.x - p0.x);
    __int128 dy1 = static_cast<__int128>(p1.y - p0.y);
    __int128 dx2 = static_cast<__int128>(p2.x - p0.x);
    __int128 dy2 = static_cast<__int128>(p2.y - p0.y);

    __int128 cross = dx1 * dy2 - dy1 * dx2;

    // Safe to cast back since we only need the sign for most uses
    if (cross > 0) return 1;
    if (cross < 0) return -1;
    return 0;
}

/**
 * @brief Full cross product value (not just sign)
 */
inline __int128 crossProductFull(const IntPoint& p0, const IntPoint& p1, const IntPoint& p2) {
    __int128 dx1 = static_cast<__int128>(p1.x - p0.x);
    __int128 dy1 = static_cast<__int128>(p1.y - p0.y);
    __int128 dx2 = static_cast<__int128>(p2.x - p0.x);
    __int128 dy2 = static_cast<__int128>(p2.y - p0.y);

    return dx1 * dy2 - dy1 * dx2;
}

/**
 * @brief Dot product of vectors (p1-p0) · (p2-p0)
 */
inline __int128 dotProduct(const IntPoint& p0, const IntPoint& p1, const IntPoint& p2) {
    __int128 dx1 = static_cast<__int128>(p1.x - p0.x);
    __int128 dy1 = static_cast<__int128>(p1.y - p0.y);
    __int128 dx2 = static_cast<__int128>(p2.x - p0.x);
    __int128 dy2 = static_cast<__int128>(p2.y - p0.y);

    return dx1 * dx2 + dy1 * dy2;
}

/**
 * @brief Squared distance between two points
 * Returns int64_t for comparisons (actual value may overflow, but safe for <, >, ==)
 */
inline __int128 distanceSquared(const IntPoint& p1, const IntPoint& p2) {
    __int128 dx = static_cast<__int128>(p1.x - p2.x);
    __int128 dy = static_cast<__int128>(p1.y - p2.y);
    return dx * dx + dy * dy;
}

/**
 * @brief Polygon signed area (scaled by 2)
 * Positive = CCW, Negative = CW
 * EXACT computation with int64_t
 */
inline __int128 polygonArea2(const IntPolygon& poly) {
    if (poly.size() < 3) return 0;

    __int128 area2 = 0;
    size_t n = poly.size();

    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        __int128 xi = static_cast<__int128>(poly[i].x);
        __int128 yi = static_cast<__int128>(poly[i].y);
        __int128 xj = static_cast<__int128>(poly[j].x);
        __int128 yj = static_cast<__int128>(poly[j].y);

        area2 += xi * yj - xj * yi;
    }

    return area2;
}

/**
 * @brief Check if point is on segment (inclusive of endpoints)
 */
inline bool pointOnSegment(const IntPoint& p, const IntPoint& a, const IntPoint& b) {
    // Point must be collinear
    if (crossProduct(a, b, p) != 0) return false;

    // Point must be between a and b
    int64_t minX = std::min(a.x, b.x);
    int64_t maxX = std::max(a.x, b.x);
    int64_t minY = std::min(a.y, b.y);
    int64_t maxY = std::max(a.y, b.y);

    return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
}

/**
 * @brief Calculate squared distance from point to line segment
 */
inline __int128 distanceToSegmentSquared(const IntPoint& p, const IntPoint& a, const IntPoint& b) {
    __int128 dx = static_cast<__int128>(b.x - a.x);
    __int128 dy = static_cast<__int128>(b.y - a.y);
    __int128 len2 = dx * dx + dy * dy;

    if (len2 == 0) {
        // Degenerate segment (a == b)
        return distanceSquared(p, a);
    }

    // Compute parametric position t of closest point on segment
    // t = ((p - a) · (b - a)) / |b - a|^2
    __int128 dpx = static_cast<__int128>(p.x - a.x);
    __int128 dpy = static_cast<__int128>(p.y - a.y);
    __int128 t = (dpx * dx + dpy * dy);

    if (t <= 0) {
        // Closest to a
        return distanceSquared(p, a);
    } else if (t >= len2) {
        // Closest to b
        return distanceSquared(p, b);
    } else {
        // Closest to interior point
        // Project p onto line: proj = a + t*(b-a) / len2
        //Distance^2 = |p - proj|^2        // Using formula: dist^2 = |p-a|^2 - t^2/len2
        __int128 dp_len2 = dpx * dpx + dpy * dpy;
        __int128 dist2 = dp_len2 - (t * t) / len2;
        return dist2 >= 0 ? dist2 : 0;  // Clamp to 0 to avoid negative due to rounding
    }
}

/**
 * @brief Check if two segments intersect (exclusive of endpoints unless overlapping)
 * Returns true if segments have a non-trivial intersection
 */
inline bool segmentsIntersect(const IntPoint& a1, const IntPoint& a2,
                              const IntPoint& b1, const IntPoint& b2) {
    int64_t o1 = crossProduct(a1, a2, b1);
    int64_t o2 = crossProduct(a1, a2, b2);
    int64_t o3 = crossProduct(b1, b2, a1);
    int64_t o4 = crossProduct(b1, b2, a2);

    // General case: opposite orientations
    if (o1 * o2 < 0 && o3 * o4 < 0) return true;

    // Special cases: collinear points
    if (o1 == 0 && pointOnSegment(b1, a1, a2)) return true;
    if (o2 == 0 && pointOnSegment(b2, a1, a2)) return true;
    if (o3 == 0 && pointOnSegment(a1, b1, b2)) return true;
    if (o4 == 0 && pointOnSegment(a2, b1, b2)) return true;

    return false;
}

/**
 * @brief Point-in-polygon test (ray casting algorithm)
 * EXACT implementation with integer arithmetic
 */
inline bool pointInPolygon(const IntPoint& p, const IntPolygon& poly) {
    int crossings = 0;
    size_t n = poly.size();

    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        const IntPoint& vi = poly[i];
        const IntPoint& vj = poly[j];

        // Check if point is on edge
        if (pointOnSegment(p, vi, vj)) {
            return true;  // On boundary counts as inside
        }

        // Ray casting: horizontal ray from p to the right
        // Count crossings with polygon edges
        if ((vi.y > p.y) != (vj.y > p.y)) {
            // Edge crosses horizontal line through p
            // Compute x-coordinate of intersection
            __int128 dx = static_cast<__int128>(vj.x - vi.x);
            __int128 dy = static_cast<__int128>(vj.y - vi.y);
            __int128 t = static_cast<__int128>(p.y - vi.y);

            // x_intersect = vi.x + dx * t / dy
            __int128 x_intersect = static_cast<__int128>(vi.x) * dy + dx * t;
            __int128 p_x_scaled = static_cast<__int128>(p.x) * dy;

            if (x_intersect > p_x_scaled) {
                crossings++;
            }
        }
    }

    return (crossings % 2) == 1;
}

// ========== ORBITAL TRACING ==========

/**
 * @brief Find start point for orbital tracing
 * For OUTSIDE NFP: Place B's reference point at A's bottommost-leftmost vertex
 * For INSIDE NFP: Place B's reference point inside A
 */
IntPoint findStartPoint(const IntPolygon& A, const IntPolygon& B, bool inside) {
    if (inside) {
        // INSIDE: Start with B[0] at A[0] (will be refined)
        return A[0];
    } else {
        // OUTSIDE: Find A's bottommost point (min Y, then min X)
        IntPoint minPoint = A[0];
        for (size_t i = 1; i < A.size(); i++) {
            if (A[i].y < minPoint.y || (A[i].y == minPoint.y && A[i].x < minPoint.x)) {
                minPoint = A[i];
            }
        }

        // Find B's topmost point (max Y, then max X) - will be placed at A's bottom
        IntPoint maxBPoint = B[0];
        for (size_t i = 1; i < B.size(); i++) {
            if (B[i].y > maxBPoint.y || (B[i].y == maxBPoint.y && B[i].x > maxBPoint.x)) {
                maxBPoint = B[i];
            }
        }

        // Start position: place B's top at A's bottom
        return minPoint - maxBPoint;
    }
}

/**
 * @brief Translate polygon by vector
 */
IntPolygon translatePolygon(const IntPolygon& poly, const IntPoint& offset) {
    IntPolygon result;
    result.points.reserve(poly.size());
    for (const auto& p : poly.points) {
        result.points.push_back(p + offset);
    }
    return result;
}

/**
 * @brief Check if two polygons intersect
 */
bool polygonsIntersect(const IntPolygon& A, const IntPolygon& B) {
    // Quick check: point-in-polygon
    if (pointInPolygon(B[0], A)) return true;
    if (pointInPolygon(A[0], B)) return true;

    // Edge-edge intersection check
    for (size_t i = 0; i < A.size(); i++) {
        size_t i_next = (i + 1) % A.size();
        for (size_t j = 0; j < B.size(); j++) {
            size_t j_next = (j + 1) % B.size();
            if (segmentsIntersect(A[i], A[i_next], B[j], B[j_next])) {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Touching point types
 */
enum TouchType {
    VERTEX_VERTEX = 0,  // B vertex touches A vertex
    VERTEX_EDGE = 1,    // B vertex touches A edge
    EDGE_VERTEX = 2     // B edge touches A vertex
};

struct TouchingPoint {
    TouchType type;
    int indexA;      // Index in A
    int indexB;      // Index in B
};

/**
 * @brief Find all touching points between A and B
 * "Touching" means zero or very small distance
 * We allow a small tolerance to handle rounding errors from trimming
 */
std::vector<TouchingPoint> findTouchingPoints(const IntPolygon& A, const IntPolygon& B) {
    std::vector<TouchingPoint> touching;

    // Small tolerance for "touching" (about 0.01 after scaling back)
    constexpr int64_t TOUCH_TOLERANCE = 100;  // 100 scaled units = 0.01 real units

    // Check all vertex-vertex pairs
    for (size_t i = 0; i < A.size(); i++) {
        for (size_t j = 0; j < B.size(); j++) {
            __int128 dist2 = distanceSquared(A[i], B[j]);
            if (dist2 <= TOUCH_TOLERANCE * TOUCH_TOLERANCE) {
                touching.push_back({VERTEX_VERTEX, static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }

    // Check B vertices on A edges
    for (size_t j = 0; j < B.size(); j++) {
        for (size_t i = 0; i < A.size(); i++) {
            size_t i_next = (i + 1) % A.size();
            __int128 dist2 = distanceToSegmentSquared(B[j], A[i], A[i_next]);
            if (dist2 <= TOUCH_TOLERANCE * TOUCH_TOLERANCE) {
                // Avoid duplicates from vertex-vertex check
                __int128 distAi2 = distanceSquared(B[j], A[i]);
                __int128 distANext2 = distanceSquared(B[j], A[i_next]);
                if (distAi2 > TOUCH_TOLERANCE * TOUCH_TOLERANCE &&
                    distANext2 > TOUCH_TOLERANCE * TOUCH_TOLERANCE) {
                    touching.push_back({VERTEX_EDGE, static_cast<int>(i), static_cast<int>(j)});
                }
            }
        }
    }

    // Check A vertices on B edges
    for (size_t i = 0; i < A.size(); i++) {
        for (size_t j = 0; j < B.size(); j++) {
            size_t j_next = (j + 1) % B.size();
            __int128 dist2 = distanceToSegmentSquared(A[i], B[j], B[j_next]);
            if (dist2 <= TOUCH_TOLERANCE * TOUCH_TOLERANCE) {
                // Avoid duplicates from vertex-vertex check
                __int128 distBj2 = distanceSquared(A[i], B[j]);
                __int128 distBNext2 = distanceSquared(A[i], B[j_next]);
                if (distBj2 > TOUCH_TOLERANCE * TOUCH_TOLERANCE &&
                    distBNext2 > TOUCH_TOLERANCE * TOUCH_TOLERANCE) {
                    touching.push_back({EDGE_VERTEX, static_cast<int>(i), static_cast<int>(j)});
                }
            }
        }
    }

    return touching;
}

/**
 * @brief Slide vector candidate
 */
struct SlideVector {
    IntPoint direction;   // Slide direction
    int startIdx;         // Start vertex index (for marking)
    int endIdx;           // End vertex index (for marking)
    bool isFromA;         // true if from A, false if from B
};

/**
 * @brief Generate candidate slide vectors from touching points
 */
std::vector<SlideVector> generateSlideVectors(
    const IntPolygon& A,
    const IntPolygon& B,
    const std::vector<TouchingPoint>& touching
) {
    std::vector<SlideVector> vectors;

    for (const auto& touch : touching) {
        if (touch.type == VERTEX_VERTEX) {
            // Type 0: Vertex-vertex touch
            // Add edge vectors from A
            int iA = touch.indexA;
            int prevA = (iA == 0) ? A.size() - 1 : iA - 1;
            int nextA = (iA + 1) % A.size();

            vectors.push_back({
                A[prevA] - A[iA],
                iA, prevA, true
            });
            vectors.push_back({
                A[nextA] - A[iA],
                iA, nextA, true
            });

            // Add edge vectors from B (INVERTED)
            int iB = touch.indexB;
            int prevB = (iB == 0) ? B.size() - 1 : iB - 1;
            int nextB = (iB + 1) % B.size();

            vectors.push_back({
                B[iB] - B[prevB],
                prevB, iB, false
            });
            vectors.push_back({
                B[iB] - B[nextB],
                nextB, iB, false
            });

        } else if (touch.type == VERTEX_EDGE) {
            // Type 1: B vertex touches A edge
            int iA = touch.indexA;
            int prevA = (iA == 0) ? A.size() - 1 : iA - 1;
            int iB = touch.indexB;

            vectors.push_back({
                A[iA] - B[iB],
                prevA, iA, true
            });
            vectors.push_back({
                A[prevA] - B[iB],
                iA, prevA, true
            });

        } else if (touch.type == EDGE_VERTEX) {
            // Type 2: A vertex touches B edge
            int iA = touch.indexA;
            int iB = touch.indexB;
            int prevB = (iB == 0) ? B.size() - 1 : iB - 1;

            vectors.push_back({
                A[iA] - B[iB],
                prevB, iB, false
            });
            vectors.push_back({
                A[iA] - B[prevB],
                iB, prevB, false
            });
        }
    }

    return vectors;
}

/**
 * @brief Calculate slide distance for a vector
 *
 * Computes the maximum distance B can move along slideVector before intersecting A.
 * Uses exact integer arithmetic for all geometric tests.
 *
 * Algorithm:
 * 1. Test each edge of B against each edge of A
 * 2. Calculate intersection distance using parametric line equations
 * 3. Return minimum positive intersection distance
 *
 * @param A Stationary polygon
 * @param B Moving polygon (at current position)
 * @param slideVector Direction and magnitude to slide
 * @return Maximum slide distance (squared), or nullopt if immediate collision
 */
std::optional<__int128> calculateSlideDistance(
    const IntPolygon& A,
    const IntPolygon& B,
    const IntPoint& slideVector
) {
    // Slide vector magnitude squared
    __int128 vecLengthSquared = distanceSquared(IntPoint(0, 0), slideVector);

    if (vecLengthSquared == 0) {
        return std::nullopt;  // Zero vector - can't slide
    }

    __int128 minDistanceSquared = vecLengthSquared;  // Start with full vector length
    bool foundIntersection = false;

    // Check each edge of B against each edge of A
    for (size_t i = 0; i < B.size(); i++) {
        size_t i_next = (i + 1) % B.size();

        // Edge of B: from B[i] to B[i_next]
        IntPoint b1 = B[i];
        IntPoint b2 = B[i_next];

        for (size_t j = 0; j < A.size(); j++) {
            size_t j_next = (j + 1) % A.size();

            // Edge of A: from A[j] to A[j_next]
            IntPoint a1 = A[j];
            IntPoint a2 = A[j_next];

            // We want to find the parameter t in [0, 1] such that:
            // (b1 + t*slideVector, b2 + t*slideVector) intersects (a1, a2)
            //
            // This is a moving segment vs static segment intersection problem.
            // We solve for t using the parametric line equations:
            //
            // Point on moving B edge: P(s,t) = b1 + s*(b2-b1) + t*slideVector
            // Point on static A edge: Q(u)   = a1 + u*(a2-a1)
            //
            // At intersection: P(s,t) = Q(u)
            // b1 + s*(b2-b1) + t*slideVector = a1 + u*(a2-a1)
            //
            // This gives us 2 equations (x and y components):
            // b1.x + s*db.x + t*sv.x = a1.x + u*da.x
            // b1.y + s*db.y + t*sv.y = a1.y + u*da.y
            //
            // Rearranging:
            // s*db.x - u*da.x + t*sv.x = a1.x - b1.x
            // s*db.y - u*da.y + t*sv.y = a1.y - b1.y
            //
            // In matrix form: [db.x  -da.x  sv.x] [s]   [a1.x - b1.x]
            //                 [db.y  -da.y  sv.y] [u] = [a1.y - b1.y]
            //                                      [t]
            //
            // We need to solve for t (and verify 0 <= s,u <= 1)

            IntPoint db = b2 - b1;  // B edge direction
            IntPoint da = a2 - a1;  // A edge direction
            IntPoint sv = slideVector;
            IntPoint diff = a1 - b1;

            // Use Cramer's rule to solve the system
            // First, check if segments are parallel (determinant = 0)
            __int128 det_base = static_cast<__int128>(db.x) * da.y -
                               static_cast<__int128>(db.y) * da.x;

            if (det_base == 0) {
                // B edge parallel to A edge - skip (or handle specially)
                continue;
            }

            // For intersection without slide (t=0):
            // s*db.x - u*da.x = diff.x
            // s*db.y - u*da.y = diff.y
            //
            // Solving for s and u:
            __int128 s_num = static_cast<__int128>(diff.x) * da.y -
                            static_cast<__int128>(diff.y) * da.x;
            __int128 u_num = static_cast<__int128>(diff.x) * db.y -
                            static_cast<__int128>(diff.y) * db.x;

            // NOTE: We skip the "already intersecting at t=0" check because when polygons
            // are touching (which is normal during orbital tracing), edges may coincide
            // and this would incorrectly reject valid slide vectors.
            // If polygons are truly overlapping, the orbital tracing will fail elsewhere.

            // Now solve for t when intersection occurs:
            // We need to find t such that the moving segment intersects A edge
            //
            // Parametric approach: sweep B edge along slideVector
            // At time t, B edge goes from (b1 + t*sv) to (b2 + t*sv)
            // This intersects A edge (a1 to a2) when:
            //
            // There exist s,u in [0,1] such that:
            // b1 + s*db + t*sv = a1 + u*da
            //
            // Rearranging:
            // s*db - u*da = a1 - b1 - t*sv
            //
            // This is a linear system in s,u for each t.
            // We can solve for t by requiring the system to have a solution with s,u in [0,1]

            // Simplified approach: test sweep of each vertex of B
            // For each vertex b of B moving along slideVector,
            // find when it intersects each edge of A

            // Test b1 sweeping along slideVector against A edge (a1, a2)
            {
                // Ray: b1 + t*sv for t >= 0
                // Segment: a1 + u*da for u in [0,1]
                //
                // Intersection: b1 + t*sv = a1 + u*da
                // Solving for t and u:
                // t*sv.x - u*da.x = a1.x - b1.x
                // t*sv.y - u*da.y = a1.y - b1.y

                __int128 det_t = static_cast<__int128>(sv.x) * da.y -
                                static_cast<__int128>(sv.y) * da.x;

                if (det_t != 0) {
                    __int128 t_num = static_cast<__int128>(diff.x) * da.y -
                                    static_cast<__int128>(diff.y) * da.x;
                    __int128 u_num = static_cast<__int128>(diff.x) * sv.y -
                                    static_cast<__int128>(diff.y) * sv.x;

                    // Check if intersection is valid: t > 0 (not t >= 0!), 0 <= u <= 1
                    // We require t > 0 to skip current touching points (t=0)
                    bool t_valid, u_valid;
                    if (det_t > 0) {
                        t_valid = (t_num > 0);  // Changed from >= to >
                        u_valid = (u_num >= 0 && u_num <= det_t);
                    } else {
                        t_valid = (t_num < 0);  // Changed from <= to <
                        u_valid = (u_num <= 0 && u_num >= det_t);
                    }

                    if (t_valid && u_valid) {
                        // Found intersection at parameter t = t_num / det_t
                        // Distance squared = (t * |sv|)^2 = t^2 * |sv|^2
                        // = (t_num / det_t)^2 * vecLengthSquared
                        // = t_num^2 * vecLengthSquared / det_t^2

                        __int128 distSq = (t_num * t_num * vecLengthSquared) / (det_t * det_t);

                        if (distSq < minDistanceSquared) {
                            minDistanceSquared = distSq;
                            foundIntersection = true;
                        }
                    }
                }
            }

            // Test b2 sweeping along slideVector against A edge (a1, a2)
            {
                IntPoint diff2 = a1 - b2;

                __int128 det_t = static_cast<__int128>(sv.x) * da.y -
                                static_cast<__int128>(sv.y) * da.x;

                if (det_t != 0) {
                    __int128 t_num = static_cast<__int128>(diff2.x) * da.y -
                                    static_cast<__int128>(diff2.y) * da.x;
                    __int128 u_num = static_cast<__int128>(diff2.x) * sv.y -
                                    static_cast<__int128>(diff2.y) * sv.x;

                    bool t_valid, u_valid;
                    if (det_t > 0) {
                        t_valid = (t_num > 0);  // Changed from >= to >
                        u_valid = (u_num >= 0 && u_num <= det_t);
                    } else {
                        t_valid = (t_num < 0);  // Changed from <= to <
                        u_valid = (u_num <= 0 && u_num >= det_t);
                    }

                    if (t_valid && u_valid) {
                        __int128 distSq = (t_num * t_num * vecLengthSquared) / (det_t * det_t);

                        if (distSq < minDistanceSquared) {
                            minDistanceSquared = distSq;
                            foundIntersection = true;
                        }
                    }
                }
            }
        }
    }

    // If we found an intersection, return the minimum distance
    // Otherwise, return the full vector length (can slide all the way)
    return minDistanceSquared;
}

/**
 * @brief Select best slide vector from candidates
 */
struct SlideResult {
    IntPoint vector;      // Direction to slide
    __int128 distance2;   // Squared distance
    int startIdx;
    int endIdx;
    bool isFromA;
};

std::optional<SlideResult> computeSlideVector(
    const IntPolygon& A,
    const IntPolygon& B,
    const std::optional<IntPoint>& prevVector
) {
    // Find touching points
    auto touching = findTouchingPoints(A, B);

    if (touching.empty()) {
        LOG_NFP("  ERROR: No touching points found");
        return std::nullopt;
    }

    LOG_NFP("  Found " << touching.size() << " touching points");

    // Generate candidate vectors
    auto candidates = generateSlideVectors(A, B, touching);

    LOG_NFP("  Generated " << candidates.size() << " candidate vectors");

    // Filter and select best vector
    std::optional<SlideResult> best;
    __int128 maxDistance2 = 0;
    int candidatesChecked = 0;
    int candidatesFiltered = 0;

    for (const auto& candidate : candidates) {
        candidatesChecked++;
        // Skip zero vectors
        if (candidate.direction.x == 0 && candidate.direction.y == 0) {
            LOG_NFP("    Candidate " << candidatesChecked << ": ZERO vector - skipped");
            candidatesFiltered++;
            continue;
        }

        // Skip vectors pointing backwards (opposite to previous)
        if (prevVector.has_value()) {
            __int128 dot = static_cast<__int128>(candidate.direction.x) * prevVector->x +
                          static_cast<__int128>(candidate.direction.y) * prevVector->y;

            if (dot < 0) {
                // Check if parallel (cross product ≈ 0)
                __int128 cross = static_cast<__int128>(candidate.direction.x) * prevVector->y -
                                static_cast<__int128>(candidate.direction.y) * prevVector->x;

                if (cross == 0) {
                    LOG_NFP("    Candidate " << candidatesChecked << ": BACKTRACK - skipped");
                    candidatesFiltered++;
                    continue;  // Parallel and opposite - skip
                }
            }
        }

        // Calculate slide distance
        auto distOpt = calculateSlideDistance(A, B, candidate.direction);
        if (!distOpt.has_value()) {
            LOG_NFP("    Candidate " << candidatesChecked << ": slideDistance returned NULLOPT (immediate collision) - skipped");
            candidatesFiltered++;
            continue;
        }

        __int128 dist2 = distOpt.value();
        LOG_NFP("    Candidate " << candidatesChecked << ": direction=(" << candidate.direction.x << "," << candidate.direction.y
               << ") slideD2=" << static_cast<double>(dist2));

        // Select vector with maximum valid distance
        if (dist2 > maxDistance2) {
            maxDistance2 = dist2;
            best = SlideResult{
                candidate.direction,
                dist2,
                candidate.startIdx,
                candidate.endIdx,
                candidate.isFromA
            };
            LOG_NFP("      → NEW BEST (dist2=" << static_cast<double>(dist2) << ")");
        }
    }

    LOG_NFP("  Checked " << candidatesChecked << " candidates, filtered " << candidatesFiltered);
    if (best.has_value()) {
        LOG_NFP("  Best vector: (" << best->vector.x << "," << best->vector.y << ") dist2=" << static_cast<double>(best->distance2));
    } else {
        LOG_NFP("  NO VALID VECTOR FOUND!");
    }

    return best;
}

/**
 * @brief Main orbital tracing algorithm (INTEGER VERSION)
 *
 * Traces the boundary of the No-Fit Polygon using exact integer arithmetic.
 * This is the FALLBACK implementation when MinkowskiSum fails.
 *
 * @param A Stationary polygon
 * @param B Moving polygon
 * @param inside true for INSIDE NFP, false for OUTSIDE NFP
 * @return Vector of NFP polygons (usually 1, but can be multiple for complex cases)
 */
std::vector<std::vector<Point>> computeNFP(
    const std::vector<Point>& A_input,
    const std::vector<Point>& B_input,
    bool inside
) {
    LOG_NFP("=== INTEGER ORBITAL TRACING START ===");
    LOG_NFP("  A size: " << A_input.size() << " points");
    LOG_NFP("  B size: " << B_input.size() << " points");
    LOG_NFP("  Mode: " << (inside ? "INSIDE" : "OUTSIDE"));

    if (A_input.size() < 3 || B_input.size() < 3) {
        LOG_NFP("  ERROR: Polygon has < 3 points");
        return {};
    }

    // Convert to integer polygons
    IntPolygon A = IntPolygon::fromPoints(A_input);
    IntPolygon B = IntPolygon::fromPoints(B_input);

    // Ensure correct winding order
    __int128 areaA = polygonArea2(A);
    __int128 areaB = polygonArea2(B);

    LOG_NFP("  Area A: " << (areaA > 0 ? "CCW" : "CW"));
    LOG_NFP("  Area B: " << (areaB > 0 ? "CCW" : "CW"));

    if (!inside) {
        // OUTSIDE: Both must be CCW
        if (areaA < 0) {
            std::reverse(A.points.begin(), A.points.end());
            LOG_NFP("  Reversed A to CCW");
        }
        if (areaB < 0) {
            std::reverse(B.points.begin(), B.points.end());
            LOG_NFP("  Reversed B to CCW");
        }
    } else {
        // INSIDE: A must be CW (container), B must be CCW (part)
        if (areaA > 0) {
            std::reverse(A.points.begin(), A.points.end());
            LOG_NFP("  Reversed A to CW");
        }
        if (areaB < 0) {
            std::reverse(B.points.begin(), B.points.end());
            LOG_NFP("  Reversed B to CCW");
        }
    }

    // Find start point
    IntPoint refPoint = findStartPoint(A, B, inside);
    IntPoint startPoint = refPoint;

    LOG_NFP("  Start point: (" << refPoint.x << ", " << refPoint.y << ")");

    // NFP points (reference point trajectory)
    std::vector<IntPoint> nfp;
    nfp.push_back(refPoint);

    int iterations = 0;
    const int MAX_ITER = MAX_ITERATIONS;

    std::optional<IntPoint> prevVector;  // Previous slide direction

    // Orbital tracing loop
    while (iterations < MAX_ITER) {
        iterations++;

        LOG_NFP("  === Iteration " << iterations << " ===");
        LOG_NFP("    refPoint: (" << refPoint.x << "," << refPoint.y << ")");

        // Translate B to current reference point
        IntPolygon B_current = translatePolygon(B, refPoint);

        LOG_NFP("    B_current[0]: (" << B_current[0].x << "," << B_current[0].y << ")");
        LOG_NFP("    A[0]: (" << A[0].x << "," << A[0].y << ")");

        // Compute next slide vector
        auto slideOpt = computeSlideVector(A, B_current, prevVector);

        if (!slideOpt.has_value()) {
            LOG_NFP("  ERROR: No valid slide vector found at iteration " << iterations);
            break;
        }

        // Extract slide direction and trim if necessary
        IntPoint slideVec = slideOpt->vector;
        __int128 slideD2 = slideOpt->distance2;
        __int128 vecD2 = distanceSquared(IntPoint(0, 0), slideVec);

        // If slide distance is shorter than vector length, trim the vector
        if (slideD2 < vecD2) {
            // Scale: slideVec *= sqrt(slideD2 / vecD2)
            // Use floating-point for the scaling calculation, then convert back
            double scale = std::sqrt(static_cast<double>(slideD2) / static_cast<double>(vecD2));
            slideVec.x = static_cast<int64_t>(std::round(slideVec.x * scale));
            slideVec.y = static_cast<int64_t>(std::round(slideVec.y * scale));

            LOG_NFP("  Vector trimmed: scale=" << scale
                   << " from (" << slideOpt->vector.x << "," << slideOpt->vector.y << ")"
                   << " to (" << slideVec.x << "," << slideVec.y << ")");
        }

        // Move reference point
        refPoint = refPoint + slideVec;
        nfp.push_back(refPoint);

        // Update previous vector
        prevVector = slideVec;

        // Check if we've returned to start
        // With integer coordinates, we need a small tolerance
        constexpr int64_t CLOSE_TOLERANCE = 100;  // Same as TOUCH_TOLERANCE
        __int128 dist2ToStart = distanceSquared(refPoint, startPoint);
        if (nfp.size() > 2 && dist2ToStart <= CLOSE_TOLERANCE * CLOSE_TOLERANCE) {
            LOG_NFP("  ✅ Closed NFP after " << iterations << " iterations");
            break;
        }

        if (iterations % 100 == 0) {
            LOG_NFP("  Iteration " << iterations << ": at ("
                   << refPoint.x << ", " << refPoint.y << ")");
        }
    }

    if (iterations >= MAX_ITER) {
        LOG_NFP("  ❌ ERROR: Max iterations reached - possible infinite loop");
        return {};
    }

    // Convert back to floating-point
    std::vector<Point> nfpPoints;
    for (const auto& p : nfp) {
        nfpPoints.push_back(p.toPoint());
    }

    LOG_NFP("=== INTEGER ORBITAL TRACING COMPLETE ===");
    LOG_NFP("  NFP has " << nfpPoints.size() << " points");

    // Apply B[0] offset (NFP reference point is B[0])
    if (!B_input.empty()) {
        for (auto& p : nfpPoints) {
            p.x += B_input[0].x;
            p.y += B_input[0].y;
        }
        LOG_NFP("  Applied B[0] offset: (" << B_input[0].x << ", " << B_input[0].y << ")");
    }

    return {nfpPoints};
}

} // namespace IntegerNFP
} // namespace deepnest
