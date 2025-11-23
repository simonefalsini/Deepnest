/**
 * @file IntegerNFP.cpp
 * @brief Integer-based No-Fit Polygon (Orbital Tracing) implementation
 *
 * DESIGN PHILOSOPHY:
 * - Uses native integer coordinates (no scaling)
 * - int32_t for coordinates, int64_t for intermediate calculations
 * - Follows computational geometry standards (CGAL, Boost.Polygon)
 * - Orbital tracing algorithm for NFP calculation
 *
 * MATHEMATICAL FOUNDATION:
 * - Cross product for orientation (exact with integers)
 * - Edge sliding with exact distance calculation
 * - No floating-point operations or tolerance checks
 */

#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/DebugConfig.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

namespace deepnest {
    namespace IntegerNFP {

        // ========== CONSTANTS ==========
        constexpr int MAX_ITERATIONS = 10000;     // Safety limit for orbital tracing

        // ========== INTEGER POINT ==========

        struct IntPoint {
            int32_t x, y;
            int index;      // Original index in polygon
            bool marked;    // For tracking visited vertices

            IntPoint() : x(0), y(0), index(-1), marked(false) {}
            IntPoint(int32_t x_, int32_t y_, int idx = -1)
                : x(x_), y(y_), index(idx), marked(false) {
            }

            // Direct conversion from Point (assumes Point already contains integer values)
            static IntPoint fromPoint(const Point& p, int idx = -1) {
                return IntPoint(
                    static_cast<int32_t>(p.x),
                    static_cast<int32_t>(p.y),
                    idx
                );
            }

            // Convert to Point
            Point toPoint() const {
                Point p;
                p.x = static_cast<double>(x);
                p.y = static_cast<double>(y);
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
         */
        inline int64_t crossProduct(const IntPoint& p0, const IntPoint& p1, const IntPoint& p2) {
            int64_t dx1 = static_cast<int64_t>(p1.x - p0.x);
            int64_t dy1 = static_cast<int64_t>(p1.y - p0.y);
            int64_t dx2 = static_cast<int64_t>(p2.x - p0.x);
            int64_t dy2 = static_cast<int64_t>(p2.y - p0.y);

            return dx1 * dy2 - dy1 * dx2;
        }

        /**
         * @brief Dot product of vectors (p1-p0) · (p2-p0)
         */
        inline int64_t dotProduct(const IntPoint& p0, const IntPoint& p1, const IntPoint& p2) {
            int64_t dx1 = static_cast<int64_t>(p1.x - p0.x);
            int64_t dy1 = static_cast<int64_t>(p1.y - p0.y);
            int64_t dx2 = static_cast<int64_t>(p2.x - p0.x);
            int64_t dy2 = static_cast<int64_t>(p2.y - p0.y);

            return dx1 * dx2 + dy1 * dy2;
        }

        /**
         * @brief Squared distance between two points
         */
        inline int64_t distanceSquared(const IntPoint& p1, const IntPoint& p2) {
            int64_t dx = static_cast<int64_t>(p1.x - p2.x);
            int64_t dy = static_cast<int64_t>(p1.y - p2.y);
            return dx * dx + dy * dy;
        }

        /**
         * @brief Polygon signed area (scaled by 2)
         * Positive = CCW, Negative = CW
         */
        inline int64_t polygonArea2(const IntPolygon& poly) {
            if (poly.size() < 3) return 0;

            int64_t area2 = 0;
            size_t n = poly.size();

            for (size_t i = 0; i < n; i++) {
                size_t j = (i + 1) % n;
                int64_t xi = static_cast<int64_t>(poly[i].x);
                int64_t yi = static_cast<int64_t>(poly[i].y);
                int64_t xj = static_cast<int64_t>(poly[j].x);
                int64_t yj = static_cast<int64_t>(poly[j].y);

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
            int32_t minX = std::min(a.x, b.x);
            int32_t maxX = std::max(a.x, b.x);
            int32_t minY = std::min(a.y, b.y);
            int32_t maxY = std::max(a.y, b.y);

            return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
        }

        /**
         * @brief Calculate squared distance from point to line segment
         */
        inline int64_t distanceToSegmentSquared(const IntPoint& p, const IntPoint& a, const IntPoint& b) {
            int64_t dx = static_cast<int64_t>(b.x - a.x);
            int64_t dy = static_cast<int64_t>(b.y - a.y);
            int64_t len2 = dx * dx + dy * dy;

            if (len2 == 0) {
                // Degenerate segment (a == b)
                return distanceSquared(p, a);
            }

            // Compute parametric position t of closest point on segment
            int64_t dpx = static_cast<int64_t>(p.x - a.x);
            int64_t dpy = static_cast<int64_t>(p.y - a.y);
            int64_t t = (dpx * dx + dpy * dy);

            if (t <= 0) {
                // Closest to a
                return distanceSquared(p, a);
            }
            else if (t >= len2) {
                // Closest to b
                return distanceSquared(p, b);
            }
            else {
                // Closest to interior point
                int64_t dp_len2 = dpx * dpx + dpy * dpy;
                int64_t dist2 = dp_len2 - (t * t) / len2;
                return dist2 >= 0 ? dist2 : 0;
            }
        }

        /**
         * @brief Check if two segments intersect
         */
        inline bool segmentsIntersect(const IntPoint& a1, const IntPoint& a2,
            const IntPoint& b1, const IntPoint& b2) {
            int64_t o1 = crossProduct(a1, a2, b1);
            int64_t o2 = crossProduct(a1, a2, b2);
            int64_t o3 = crossProduct(b1, b2, a1);
            int64_t o4 = crossProduct(b1, b2, a2);

            // General case: opposite orientations
            if ((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) {
                if ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0)) {
                    return true;
                }
            }

            // Special cases: collinear points
            if (o1 == 0 && pointOnSegment(b1, a1, a2)) return true;
            if (o2 == 0 && pointOnSegment(b2, a1, a2)) return true;
            if (o3 == 0 && pointOnSegment(a1, b1, b2)) return true;
            if (o4 == 0 && pointOnSegment(a2, b1, b2)) return true;

            return false;
        }

        /**
         * @brief Point-in-polygon test (ray casting algorithm)
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
                if ((vi.y > p.y) != (vj.y > p.y)) {
                    // Edge crosses horizontal line through p
                    int64_t dx = static_cast<int64_t>(vj.x - vi.x);
                    int64_t dy = static_cast<int64_t>(vj.y - vi.y);
                    int64_t t = static_cast<int64_t>(p.y - vi.y);

                    int64_t x_intersect = static_cast<int64_t>(vi.x) * dy + dx * t;
                    int64_t p_x_scaled = static_cast<int64_t>(p.x) * dy;

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
         */
        IntPoint findStartPoint(const IntPolygon& A, const IntPolygon& B, bool inside) {
            if (inside) {
                // INSIDE: Start with B[0] at A[0]
                return A[0];
            }
            else {
                // OUTSIDE: Find A's bottommost point (min Y, then min X)
                IntPoint minPoint = A[0];
                for (size_t i = 1; i < A.size(); i++) {
                    if (A[i].y < minPoint.y || (A[i].y == minPoint.y && A[i].x < minPoint.x)) {
                        minPoint = A[i];
                    }
                }

                // Find B's topmost point (max Y, then max X)
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
            VERTEX_VERTEX = 0,
            VERTEX_EDGE = 1,
            EDGE_VERTEX = 2
        };

        struct TouchingPoint {
            TouchType type;
            int indexA;
            int indexB;
        };

        /**
         * @brief Find all touching points between A and B
         */
        std::vector<TouchingPoint> findTouchingPoints(const IntPolygon& A, const IntPolygon& B) {
            std::vector<TouchingPoint> touching;

            // Check all vertex-vertex pairs (exact match)
            for (size_t i = 0; i < A.size(); i++) {
                for (size_t j = 0; j < B.size(); j++) {
                    if (A[i] == B[j]) {
                        touching.push_back({ VERTEX_VERTEX, static_cast<int>(i), static_cast<int>(j) });
                    }
                }
            }

            // Check B vertices on A edges
            for (size_t j = 0; j < B.size(); j++) {
                for (size_t i = 0; i < A.size(); i++) {
                    size_t i_next = (i + 1) % A.size();
                    if (pointOnSegment(B[j], A[i], A[i_next])) {
                        // Avoid duplicates from vertex-vertex check
                        if (B[j] != A[i] && B[j] != A[i_next]) {
                            touching.push_back({ VERTEX_EDGE, static_cast<int>(i), static_cast<int>(j) });
                        }
                    }
                }
            }

            // Check A vertices on B edges
            for (size_t i = 0; i < A.size(); i++) {
                for (size_t j = 0; j < B.size(); j++) {
                    size_t j_next = (j + 1) % B.size();
                    if (pointOnSegment(A[i], B[j], B[j_next])) {
                        // Avoid duplicates from vertex-vertex check
                        if (A[i] != B[j] && A[i] != B[j_next]) {
                            touching.push_back({ EDGE_VERTEX, static_cast<int>(i), static_cast<int>(j) });
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
            IntPoint direction;
            int startIdx;
            int endIdx;
            bool isFromA;
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

                }
                else if (touch.type == VERTEX_EDGE) {
                    // Type 1: B vertex touches A edge
                    int iA = touch.indexA;
                    int nextA = (iA + 1) % A.size();
                    int iB = touch.indexB;

                    vectors.push_back({
                        A[nextA] - A[iA],
                        iA, nextA, true
                        });

                }
                else if (touch.type == EDGE_VERTEX) {
                    // Type 2: A vertex touches B edge
                    int iA = touch.indexA;
                    int iB = touch.indexB;
                    int nextB = (iB + 1) % B.size();

                    vectors.push_back({
                        B[iB] - B[nextB],
                        nextB, iB, false
                        });
                }
            }

            return vectors;
        }

        /**
         * @brief Calculate slide distance for a vector
         */
        std::optional<int64_t> calculateSlideDistance(
            const IntPolygon& A,
            const IntPolygon& B,
            const IntPoint& slideVector
        ) {
            int64_t vecLengthSquared = distanceSquared(IntPoint(0, 0), slideVector);

            if (vecLengthSquared == 0) {
                return std::nullopt;
            }

            int64_t minDistanceSquared = vecLengthSquared;
            bool foundIntersection = false;

            // Check each edge of B against each edge of A
            for (size_t i = 0; i < B.size(); i++) {
                size_t i_next = (i + 1) % B.size();
                IntPoint b1 = B[i];
                IntPoint b2 = B[i_next];

                for (size_t j = 0; j < A.size(); j++) {
                    size_t j_next = (j + 1) % A.size();
                    IntPoint a1 = A[j];
                    IntPoint a2 = A[j_next];

                    IntPoint db = b2 - b1;
                    IntPoint da = a2 - a1;
                    IntPoint sv = slideVector;

                    // Test b1 sweeping along slideVector against A edge
                    {
                        IntPoint diff = a1 - b1;
                        int64_t det_t = static_cast<int64_t>(sv.x) * da.y -
                            static_cast<int64_t>(sv.y) * da.x;

                        if (det_t != 0) {
                            int64_t t_num = static_cast<int64_t>(diff.x) * da.y -
                                static_cast<int64_t>(diff.y) * da.x;
                            int64_t u_num = static_cast<int64_t>(diff.x) * sv.y -
                                static_cast<int64_t>(diff.y) * sv.x;

                            bool t_valid, u_valid;
                            if (det_t > 0) {
                                t_valid = (t_num > 0);
                                u_valid = (u_num >= 0 && u_num <= det_t);
                            }
                            else {
                                t_valid = (t_num < 0);
                                u_valid = (u_num <= 0 && u_num >= det_t);
                            }

                            if (t_valid && u_valid) {
                                int64_t distSq = (t_num * t_num * vecLengthSquared) / (det_t * det_t);

                                if (distSq < minDistanceSquared) {
                                    minDistanceSquared = distSq;
                                    foundIntersection = true;
                                }
                            }
                        }
                    }

                    // Test b2 sweeping along slideVector against A edge
                    {
                        IntPoint diff2 = a1 - b2;
                        int64_t det_t = static_cast<int64_t>(sv.x) * da.y -
                            static_cast<int64_t>(sv.y) * da.x;

                        if (det_t != 0) {
                            int64_t t_num = static_cast<int64_t>(diff2.x) * da.y -
                                static_cast<int64_t>(diff2.y) * da.x;
                            int64_t u_num = static_cast<int64_t>(diff2.x) * sv.y -
                                static_cast<int64_t>(diff2.y) * sv.x;

                            bool t_valid, u_valid;
                            if (det_t > 0) {
                                t_valid = (t_num > 0);
                                u_valid = (u_num >= 0 && u_num <= det_t);
                            }
                            else {
                                t_valid = (t_num < 0);
                                u_valid = (u_num <= 0 && u_num >= det_t);
                            }

                            if (t_valid && u_valid) {
                                int64_t distSq = (t_num * t_num * vecLengthSquared) / (det_t * det_t);

                                if (distSq < minDistanceSquared) {
                                    minDistanceSquared = distSq;
                                    foundIntersection = true;
                                }
                            }
                        }
                    }
                }
            }

            return minDistanceSquared;
        }

        /**
         * @brief Select best slide vector from candidates
         */
        struct SlideResult {
            IntPoint vector;
            int64_t distance2;
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
            int64_t maxDistance2 = 0;

            for (const auto& candidate : candidates) {
                // Skip zero vectors
                if (candidate.direction.x == 0 && candidate.direction.y == 0) {
                    continue;
                }

                // Skip vectors pointing backwards
                if (prevVector.has_value()) {
                    int64_t dot = static_cast<int64_t>(candidate.direction.x) * prevVector->x +
                        static_cast<int64_t>(candidate.direction.y) * prevVector->y;

                    if (dot < 0) {
                        int64_t cross = static_cast<int64_t>(candidate.direction.x) * prevVector->y -
                            static_cast<int64_t>(candidate.direction.y) * prevVector->x;

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

                int64_t dist2 = distOpt.value();

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
         * @brief Scale vector to specific length (integer approximation)
         */
        IntPoint scaleVector(const IntPoint& vec, int64_t targetLength2, int64_t currentLength2) {
            if (currentLength2 == 0 || targetLength2 >= currentLength2) {
                return vec;
            }

            // Use integer approximation: scale ≈ sqrt(target/current)
            // To avoid floating-point, we scale by the ratio and then adjust
            int64_t scale2 = (targetLength2 * 1000000) / currentLength2;  // Scale squared * 10^6
            int32_t scale = 1000;  // Start with 1.0 * 1000

            // Binary search for approximate square root
            int32_t low = 0, high = 1000;
            while (low < high - 1) {
                int32_t mid = (low + high) / 2;
                if (static_cast<int64_t>(mid) * mid > scale2) {
                    high = mid;
                }
                else {
                    low = mid;
                }
            }
            scale = low;

            // Apply scale
            return IntPoint(
                (vec.x * scale) / 1000,
                (vec.y * scale) / 1000
            );
        }

        /**
         * @brief Main orbital tracing algorithm
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
            int64_t areaA = polygonArea2(A);
            int64_t areaB = polygonArea2(B);

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
            }
            else {
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

            std::optional<IntPoint> prevVector;

            // Orbital tracing loop
            while (iterations < MAX_ITER) {
                iterations++;

                LOG_NFP("  === Iteration " << iterations << " ===");
                LOG_NFP("    refPoint: (" << refPoint.x << "," << refPoint.y << ")");

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
                int64_t slideD2 = slideOpt->distance2;
                int64_t vecD2 = distanceSquared(IntPoint(0, 0), slideVec);

                // If slide distance is shorter than vector length, trim the vector
                if (slideD2 < vecD2) {
                    slideVec = scaleVector(slideVec, slideD2, vecD2);
                    LOG_NFP("  Vector trimmed from (" << slideOpt->vector.x << "," << slideOpt->vector.y
                        << ") to (" << slideVec.x << "," << slideVec.y << ")");
                }

                // Move reference point
                refPoint = refPoint + slideVec;
                nfp.push_back(refPoint);

                // Update previous vector
                prevVector = slideVec;

                // Check if we've returned to start (exact match for integers)
                if (nfp.size() > 2 && refPoint == startPoint) {
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

            return { nfpPoints };
        }

    } // namespace IntegerNFP
} // namespace deepnest