#include "../../include/deepnest/geometry/OrbitalTypes.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include <vector>
#include <cmath>

namespace deepnest {

// Use GeometryUtil functions without namespace prefix
using namespace GeometryUtil;

namespace GeometryUtil {

/**
 * @brief Find all touching contacts between polygons A and B
 *
 * Detects three types of contact:
 * - Type 0: Vertex-to-vertex contact
 * - Type 1: Vertex of B lies on edge of A
 * - Type 2: Vertex of A lies on edge of B
 *
 * Corresponds to JavaScript code in geometryutil.js lines 1506-1520
 *
 * @param A First polygon
 * @param B Second polygon (already translated by offsetB)
 * @param offsetB Current offset of polygon B
 * @return Vector of touching contacts
 */
std::vector<TouchingContact> findTouchingContacts(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& offsetB)
{
    std::vector<TouchingContact> touching;

    for (size_t i = 0; i < A.size(); i++) {
        size_t nexti = (i + 1) % A.size();

        for (size_t j = 0; j < B.size(); j++) {
            size_t nextj = (j + 1) % B.size();

            // Translate B vertices by current offset
            Point Bj_translated(B[j].x + offsetB.x, B[j].y + offsetB.y);
            Point Bnextj_translated(B[nextj].x + offsetB.x, B[nextj].y + offsetB.y);

            // Type 0: Vertex-to-vertex contact
            // JavaScript: if(_almostEqual(A[i].x, B[j].x+B.offsetx) && _almostEqual(A[i].y, B[j].y+B.offsety))
            if (almostEqual(A[i].x, Bj_translated.x) && almostEqual(A[i].y, Bj_translated.y)) {
                touching.push_back({TouchingType::VERTEX_VERTEX, (int)i, (int)j});
            }
            // Type 1: B vertex lies on A edge
            // JavaScript: else if(_onSegment(A[i],A[nexti],{x: B[j].x+B.offsetx, y: B[j].y + B.offsety}))
            else if (onSegment(A[i], A[nexti], Bj_translated)) {
                touching.push_back({TouchingType::VERTEX_ON_EDGE_A, (int)nexti, (int)j});
            }
            // Type 2: A vertex lies on B edge
            // JavaScript: else if(_onSegment({x: B[j].x+B.offsetx, ...},{x: B[nextj].x+B.offsetx, ...},A[i]))
            else if (onSegment(Bj_translated, Bnextj_translated, A[i])) {
                touching.push_back({TouchingType::VERTEX_ON_EDGE_B, (int)i, (int)nextj});
            }
        }
    }

    return touching;
}

/**
 * @brief Generate translation vectors from a touching contact
 *
 * Based on the type of contact, generates 2-4 translation vectors that
 * represent possible directions to slide polygon B along polygon A.
 *
 * - Type 0 (vertex-vertex): 4 vectors (2 from A edges, 2 from B edges inverted)
 * - Type 1 (B on A edge): 2 vectors along A edge
 * - Type 2 (A on B edge): 2 vectors along B edge
 *
 * Corresponds to JavaScript code in geometryutil.js lines 1550-1615
 *
 * @param touch The touching contact to process
 * @param A First polygon
 * @param B Second polygon (NOT translated)
 * @param offsetB Current offset of polygon B
 * @return Vector of translation vectors
 */
std::vector<TranslationVector> generateTranslationVectors(
    const TouchingContact& touch,
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    const Point& offsetB)
{
    std::vector<TranslationVector> vectors;

    // Get adjacent vertex indices (with wrapping)
    int prevAindex = (touch.indexA - 1 + A.size()) % A.size();
    int nextAindex = (touch.indexA + 1) % A.size();
    int prevBindex = (touch.indexB - 1 + B.size()) % B.size();
    int nextBindex = (touch.indexB + 1) % B.size();

    const Point& vertexA = A[touch.indexA];
    const Point& prevA = A[prevAindex];
    const Point& nextA = A[nextAindex];
    const Point& vertexB = B[touch.indexB];
    const Point& prevB = B[prevBindex];
    const Point& nextB = B[nextBindex];

    if (touch.type == TouchingType::VERTEX_VERTEX) {
        // Type 0: Vertex-to-vertex contact
        // Generate 4 vectors: 2 from A edges, 2 from B edges (inverted)

        // JavaScript lines 1552-1557: vA1 = prevA - vertexA
        vectors.push_back({
            prevA.x - vertexA.x,
            prevA.y - vertexA.y,
            (int)touch.indexA,
            prevAindex,
            'A'
        });

        // JavaScript lines 1559-1564: vA2 = nextA - vertexA
        vectors.push_back({
            nextA.x - vertexA.x,
            nextA.y - vertexA.y,
            (int)touch.indexA,
            nextAindex,
            'A'
        });

        // JavaScript lines 1566-1572: vB1 = vertexB - prevB (INVERTED!)
        vectors.push_back({
            vertexB.x - prevB.x,
            vertexB.y - prevB.y,
            prevBindex,
            (int)touch.indexB,
            'B'
        });

        // JavaScript lines 1574-1579: vB2 = vertexB - nextB (INVERTED!)
        vectors.push_back({
            vertexB.x - nextB.x,
            vertexB.y - nextB.y,
            nextBindex,
            (int)touch.indexB,
            'B'
        });
    }
    else if (touch.type == TouchingType::VERTEX_ON_EDGE_A) {
        // Type 1: B vertex lies on A edge
        // Generate 2 vectors along the A edge

        Point Bj_translated(vertexB.x + offsetB.x, vertexB.y + offsetB.y);

        // JavaScript lines 1587-1592: vertexA - (vertexB + offset)
        vectors.push_back({
            vertexA.x - Bj_translated.x,
            vertexA.y - Bj_translated.y,
            prevAindex,
            (int)touch.indexA,
            'A'
        });

        // JavaScript lines 1594-1599: prevA - (vertexB + offset)
        vectors.push_back({
            prevA.x - Bj_translated.x,
            prevA.y - Bj_translated.y,
            (int)touch.indexA,
            prevAindex,
            'A'
        });
    }
    else if (touch.type == TouchingType::VERTEX_ON_EDGE_B) {
        // Type 2: A vertex lies on B edge
        // Generate 2 vectors along the B edge

        Point Bj_translated(vertexB.x + offsetB.x, vertexB.y + offsetB.y);
        Point prevB_translated(prevB.x + offsetB.x, prevB.y + offsetB.y);

        // JavaScript lines 1602-1607: vertexA - (vertexB + offset)
        vectors.push_back({
            vertexA.x - Bj_translated.x,
            vertexA.y - Bj_translated.y,
            prevBindex,
            (int)touch.indexB,
            'B'
        });

        // JavaScript lines 1609-1614: vertexA - (prevB + offset)
        vectors.push_back({
            vertexA.x - prevB_translated.x,
            vertexA.y - prevB_translated.y,
            (int)touch.indexB,
            prevBindex,
            'B'
        });
    }

    return vectors;
}

/**
 * @brief Check if a vector represents backtracking (going back the way we came)
 *
 * A vector is considered backtracking if:
 * 1. It's a zero vector, OR
 * 2. It points in the opposite direction to prevVector (dot product < 0) AND
 *    is collinear with prevVector (cross product magnitude < 0.0001)
 *
 * Corresponds to JavaScript code in geometryutil.js lines 1624-1642
 *
 * @param vec Vector to check
 * @param prevVector Previous translation vector (null on first iteration)
 * @return true if this vector represents backtracking
 */
bool isBacktracking(
    const TranslationVector& vec,
    const std::optional<TranslationVector>& prevVector)
{
    // JavaScript line 1624: if(vectors[i].x == 0 && vectors[i].y == 0)
    if (vec.isZero()) {
        return true;
    }

    // If no previous vector, can't be backtracking
    if (!prevVector.has_value()) {
        return false;
    }

    // JavaScript line 1630: if(prevvector && vectors[i].y * prevvector.y + vectors[i].x * prevvector.x < 0)
    double dotProduct = vec.x * prevVector->x + vec.y * prevVector->y;

    if (dotProduct < 0) {
        // Vectors point in opposite directions, check if collinear

        // Normalize to unit vectors for comparison
        double vecLen = vec.length();
        double prevLen = prevVector->length();

        if (vecLen < TOL || prevLen < TOL) {
            return false;  // Degenerate vector
        }

        // JavaScript lines 1633-1637: compute unit vectors
        double unitVecX = vec.x / vecLen;
        double unitVecY = vec.y / vecLen;
        double prevUnitX = prevVector->x / prevLen;
        double prevUnitY = prevVector->y / prevLen;

        // JavaScript line 1640: check cross product magnitude
        double crossMag = std::abs(unitVecX * prevUnitY - unitVecY * prevUnitX);

        if (crossMag < 0.0001) {
            // Vectors are collinear and opposite direction → backtracking
            return true;
        }
    }

    return false;
}

} // namespace GeometryUtil

} // namespace deepnest
