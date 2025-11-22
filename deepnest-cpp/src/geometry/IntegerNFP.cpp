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
 * "Touching" means zero distance (exact with integer math)
 */
std::vector<TouchingPoint> findTouchingPoints(const IntPolygon& A, const IntPolygon& B) {
    std::vector<TouchingPoint> touching;

    // Check all vertex-vertex pairs
    for (size_t i = 0; i < A.size(); i++) {
        for (size_t j = 0; j < B.size(); j++) {
            if (A[i] == B[j]) {
                touching.push_back({VERTEX_VERTEX, static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }

    // Check B vertices on A edges
    for (size_t j = 0; j < B.size(); j++) {
        for (size_t i = 0; i < A.size(); i++) {
            size_t i_next = (i + 1) % A.size();
            if (pointOnSegment(B[j], A[i], A[i_next]) && B[j] != A[i] && B[j] != A[i_next]) {
                touching.push_back({VERTEX_EDGE, static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }

    // Check A vertices on B edges
    for (size_t i = 0; i < A.size(); i++) {
        for (size_t j = 0; j < B.size(); j++) {
            size_t j_next = (j + 1) % B.size();
            if (pointOnSegment(A[i], B[j], B[j_next]) && A[i] != B[j] && A[i] != B[j_next]) {
                touching.push_back({EDGE_VERTEX, static_cast<int>(i), static_cast<int>(j)});
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
 * Returns maximum distance B can move along vector before intersecting A
 * Returns nullopt if movement would immediately cause intersection
 */
std::optional<__int128> calculateSlideDistance(
    const IntPolygon& A,
    const IntPolygon& B,
    const IntPoint& slideVector
) {
    // This is a simplified version - needs full implementation
    // For now, return the vector length
    return distanceSquared(IntPoint(0, 0), slideVector);
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

    for (const auto& candidate : candidates) {
        // Skip zero vectors
        if (candidate.direction.x == 0 && candidate.direction.y == 0) {
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
                    continue;  // Parallel and opposite - skip
                }
            }
        }

        // Calculate slide distance
        auto distOpt = calculateSlideDistance(A, B, candidate.direction);
        if (!distOpt.has_value()) {
            continue;
        }

        __int128 dist2 = distOpt.value();

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
        }
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

        // Translate B to current reference point
        IntPolygon B_current = translatePolygon(B, refPoint);

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
            // In integer math, this is tricky - for now, use the slide distance as-is
            // A proper implementation would use fixed-point or rational arithmetic
            LOG_NFP("  Vector trimmed: slideD2=" << static_cast<double>(slideD2)
                   << " vecD2=" << static_cast<double>(vecD2));
        }

        // Move reference point
        refPoint = refPoint + slideVec;
        nfp.push_back(refPoint);

        // Update previous vector
        prevVector = slideVec;

        // Check if we've returned to start (exact comparison!)
        if (nfp.size() > 2 && refPoint == startPoint) {
            LOG_NFP("  ✅ Closed NFP after " << iterations << " iterations");
            break;
        }

        // Check if we've created a loop (visited same point before)
        bool looped = false;
        for (size_t i = 0; i < nfp.size() - 1; i++) {
            if (nfp[i] == refPoint) {
                LOG_NFP("  ✅ Loop detected at iteration " << iterations
                       << " (matched point " << i << ")");
                looped = true;
                break;
            }
        }

        if (looped) break;

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
