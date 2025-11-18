#ifndef DEEPNEST_MERGE_DETECTION_H
#define DEEPNEST_MERGE_DETECTION_H

#include "../core/Point.h"
#include "../core/Polygon.h"
#include <vector>
#include <utility>

namespace deepnest {

/**
 * @brief Merge lines detection for optimization
 *
 * This class detects parallel/aligned line segments between placed parts
 * and a new part being placed. Merged lines represent opportunities for
 * efficient cutting operations.
 *
 * The algorithm:
 * 1. For each edge of the new part
 * 2. Rotate coordinate system to make the edge horizontal
 * 3. Check all edges of placed parts for alignment (within tolerance)
 * 4. Calculate overlap length between aligned segments
 * 5. Sum total merged length across all aligned segments
 *
 * References:
 * - background.js: mergedLength function (lines 352-487)
 */
class MergeDetection {
public:
    /**
     * @brief Result structure for merge detection
     */
    struct MergeResult {
        /**
         * @brief Total length of all merged segments
         */
        double totalLength;

        /**
         * @brief List of merged line segments (world coordinates)
         * Each pair represents start and end points of a merged segment
         */
        std::vector<std::pair<Point, Point>> segments;

        MergeResult() : totalLength(0.0) {}
    };

    /**
     * @brief Calculate merged length between placed parts and new part
     *
     * Detects aligned line segments between the new part and all placed parts,
     * calculating the total length of overlapping segments. This helps optimize
     * cutting operations by identifying shared edges.
     *
     * Algorithm:
     * - Iterate through edges of newPart
     * - Skip edges shorter than minLength
     * - Skip edges with non-exact vertices
     * - For each edge, rotate coordinate system to make it horizontal
     * - Check all edges of placed parts for alignment (y ~= 0 after rotation)
     * - Calculate x-axis overlap between aligned segments
     * - Transform overlap back to world coordinates
     * - Recursively process children polygons
     *
     * @param placed Vector of placed polygons (with positions)
     * @param newPart New part being checked (should be positioned)
     * @param minLength Minimum edge length to consider (filter threshold)
     * @param tolerance Floating-point tolerance for alignment detection
     * @return MergeResult containing total length and segment list
     *
     * References:
     * - background.js line 352: function mergedLength(parts, p, minlength, tolerance)
     */
    static MergeResult calculateMergedLength(
        const std::vector<Polygon>& placed,
        const Polygon& newPart,
        double minLength,
        double tolerance
    );

private:
    /**
     * @brief Helper to calculate merged length recursively
     *
     * Internal implementation that handles recursive processing of children.
     *
     * @param parts Vector of placed parts (may have children)
     * @param p Points of the part being checked
     * @param minLength Minimum edge length threshold
     * @param tolerance Alignment tolerance
     * @return MergeResult with total length and segments
     */
    static MergeResult calculateMergedLengthInternal(
        const std::vector<Polygon>& parts,
        const std::vector<Point>& p,
        double minLength,
        double tolerance
    );

    /**
     * @brief Check if two values are almost equal within tolerance
     *
     * @param a First value
     * @param b Second value
     * @param tolerance Maximum allowed difference
     * @return true if |a - b| < tolerance
     */
    static bool almostEqual(double a, double b, double tolerance);
};

} // namespace deepnest

#endif // DEEPNEST_MERGE_DETECTION_H
