#ifndef DEEPNEST_NFP_CALCULATOR_H
#define DEEPNEST_NFP_CALCULATOR_H

#include "../core/Polygon.h"
#include "../config/DeepNestConfig.h"
#include "NFPCache.h"
#include <vector>
#include <memory>

namespace deepnest {

/**
 * @brief High-level NFP (No-Fit Polygon) calculator
 *
 * This class provides the main interface for calculating NFPs between polygons.
 * It integrates MinkowskiSum for computation and NFPCache for performance optimization.
 *
 * References:
 * - background.js: getOuterNfp, getInnerNfp, getFrame
 * - geometryutil.js: noFitPolygon
 */
class NFPCalculator {
private:
    NFPCache& cache_;

    /**
     * @brief Compute NFP without cache lookup
     * @param A Stationary polygon
     * @param B Moving polygon
     * @param inside If true, compute inner NFP; if false, compute outer NFP
     * @return Computed NFP polygon
     */
    Polygon computeDiffNFP(const Polygon& A, const Polygon& B) const;
    /**
     * @brief Compute NFP 
     *
     *
     * @param A Stationary polygon (may have children/holes)
     * @param B Moving polygon
     * @return Computed NFP polygon
     */
    Polygon computeNFP(const Polygon& A, const Polygon& B) const;

    /**
     * @brief Create a rectangular frame around a polygon
     * @param A The polygon to frame
     * @return Frame polygon with A as a child (hole)
     *
     * The frame bounds are expanded by 10% as in the original JavaScript code
     */
    Polygon createFrame(const Polygon& A) const;

public:
    /**
     * @brief Constructor
     * @param cache Reference to NFP cache for storing/retrieving results
     */
    explicit NFPCalculator(NFPCache& cache);

    /**
     * @brief Calculate outer NFP between two polygons
     *
     * The outer NFP represents the locus of positions where polygon B can be placed
     * such that it touches but does not overlap polygon A.
     *
     * @param A Stationary polygon
     * @param B Moving polygon
     * @param inside If true, B orbits inside A; if false, B orbits outside A
     * @return Outer NFP polygon (empty if calculation fails)
     *
     * Corresponds to background.js getOuterNfp()
     */
    Polygon getOuterNFP(const Polygon& A, const Polygon& B, bool inside = false);

    /**
     * @brief Calculate inner NFP for placing B inside A
     *
     * The inner NFP represents valid positions where B can be placed completely
     * inside A without overlapping.
     *
     * @param A Container polygon (stationary)
     * @param B Polygon to be placed inside A
     * @return List of inner NFP polygons (may be multiple regions or empty)
     *
     * Corresponds to background.js getInnerNfp()
     */
    std::vector<Polygon> getInnerNFP(const Polygon& A, const Polygon& B);

    /**
     * @brief Get the rectangular frame for a polygon
     *
     * Creates a rectangular boundary around polygon A, expanded by 10%.
     * The original polygon A becomes a child (hole) of the frame.
     *
     * @param A The polygon to frame
     * @return Frame polygon with A as child
     *
     * Corresponds to background.js getFrame()
     */
    Polygon getFrame(const Polygon& A) const;

    /**
     * @brief Clear the NFP cache
     *
     * Useful for freeing memory or forcing recalculation
     */
    void clearCache();

    /**
     * @brief Get cache statistics
     * @return Tuple of (hits, misses, size)
     */
    std::tuple<size_t, size_t, size_t> getCacheStats() const;
};

} // namespace deepnest

#endif // DEEPNEST_NFP_CALCULATOR_H
