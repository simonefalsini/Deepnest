#ifndef DEEPNEST_ORBITAL_TYPES_H
#define DEEPNEST_ORBITAL_TYPES_H

#include "../core/Point.h"
#include "../core/Types.h"
#include <cmath>

namespace deepnest {

/**
 * @brief Types of touching contact between two polygons
 *
 * Based on JavaScript implementation in geometryutil.js (line 1511-1518)
 */
enum class TouchingType {
    VERTEX_VERTEX = 0,     // Vertex of A touches vertex of B
    VERTEX_ON_EDGE_A = 1,  // Vertex of B lies on edge of A
    VERTEX_ON_EDGE_B = 2   // Vertex of A lies on edge of B
};

/**
 * @brief Represents a touching contact point between polygons A and B
 *
 * Corresponds to JavaScript touching array elements with {type, A, B}
 * Reference: geometryutil.js lines 1511-1518
 */
struct TouchingContact {
    TouchingType type;
    int indexA;    // Index of vertex in polygon A
    int indexB;    // Index of vertex in polygon B
};

/**
 * @brief Represents a translation vector for polygon sliding
 *
 * This structure holds both the direction vector and metadata about
 * which edge/vertices it originated from. Used for tracking the
 * orbital path around the NFP boundary.
 *
 * Corresponds to JavaScript vectors array with {x, y, start, end}
 * Reference: geometryutil.js lines 1552-1615
 */
struct TranslationVector {
    double x, y;           // Direction vector components
    int startIndex;        // Index of start vertex (for marking)
    int endIndex;          // Index of end vertex (for marking)
    char polygon;          // 'A' or 'B' - which polygon this edge belongs to

    /**
     * @brief Calculate the length (magnitude) of this vector
     */
    double length() const {
        return std::sqrt(x * x + y * y);
    }

    /**
     * @brief Normalize this vector to unit length
     */
    void normalize() {
        double len = length();
        if (len > TOL) {
            x /= len;
            y /= len;
        }
    }

    /**
     * @brief Check if this is a zero vector
     */
    bool isZero() const {
        return std::abs(x) < TOL && std::abs(y) < TOL;
    }
};

} // namespace deepnest

#endif // DEEPNEST_ORBITAL_TYPES_H
