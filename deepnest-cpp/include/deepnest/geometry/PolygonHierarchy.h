#ifndef DEEPNEST_POLYGON_HIERARCHY_H
#define DEEPNEST_POLYGON_HIERARCHY_H

#include "../core/Polygon.h"
#include <vector>

namespace deepnest {

/**
 * @brief Utility class for building polygon hierarchies
 *
 * This class provides functionality to automatically construct parent-child
 * relationships between polygons based on geometric containment.
 * 
 * Corresponds to JavaScript toTree() function in svgnest.js lines 541-591
 */
class PolygonHierarchy {
public:
    /**
     * @brief Build hierarchical tree from flat list of polygons
     *
     * Algorithm (from svgnest.js lines 541-591):
     * 1. For each polygon, check if its first point is inside another polygon
     * 2. If yes, it becomes a child of that polygon (hole)
     * 3. If no, it becomes a parent (top-level polygon)
     * 4. Assign unique IDs to parent polygons
     * 5. Recursively process children
     *
     * @param polygons Flat list of polygons (will be modified in-place)
     * @param idStart Starting ID for parent polygons (default: 0)
     * @return Vector of top-level polygons with children nested
     *
     * Example:
     * ```cpp
     * std::vector<Polygon> polygons = loadFromSVG("file.svg");
     * std::vector<Polygon> tree = PolygonHierarchy::buildTree(polygons);
     * // tree now contains only top-level polygons
     * // holes are in polygon.children
     * ```
     */
    static std::vector<Polygon> buildTree(
        std::vector<Polygon>& polygons,
        int idStart = 0
    );

private:
    /**
     * @brief Recursive helper for building tree
     *
     * @param list List of polygons to process
     * @param id Current ID counter
     * @return Next available ID after processing
     */
    static int buildTreeRecursive(
        std::vector<Polygon>& list,
        int id
    );
};

} // namespace deepnest

#endif // DEEPNEST_POLYGON_HIERARCHY_H
