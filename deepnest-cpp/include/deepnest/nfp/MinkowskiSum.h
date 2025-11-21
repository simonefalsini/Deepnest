#ifndef DEEPNEST_MINKOWSKISUM_H
#define DEEPNEST_MINKOWSKISUM_H

#include "../core/Polygon.h"
#include <vector>

namespace deepnest {

/**
 * @brief Minkowski Sum calculation for NFP
 *
 * This class provides Minkowski sum calculation using Boost.Polygon's
 * convolve functions. The Minkowski sum is used to compute the No-Fit
 * Polygon (NFP) between two shapes.
 *
 * Based on minkowski.cc from the original DeepNest
 */
class MinkowskiSum {
public:
    /**
     * @brief Calculate No-Fit Polygon using Minkowski sum
     *
     * Computes the NFP of polygon B sliding around polygon A.
     * The result is a polygon (or set of polygons) that represents
     * all valid positions where B's reference point can be placed
     * such that B touches but does not overlap with A.
     *
     * @param A First polygon (stationary)
     * @param B Second polygon (moving)
     * @param inner If true, B can slide inside A; if false, outside
     * @return Vector of NFP polygons
     */
    static std::vector<Polygon> calculateNFP(
        const Polygon& A,
        const Polygon& B,
        bool inner = false
    );

    /**
     * @brief Calculate NFPs for multiple B polygons against one A
     *
     * Batch version for efficiency when computing NFPs of multiple
     * polygons against the same stationary polygon.
     *
     * @param A Stationary polygon
     * @param Blist List of moving polygons
     * @param inner If true, polygons can slide inside; if false, outside
     * @return Vector of NFP results (one per B polygon)
     */
    static std::vector<std::vector<Polygon>> calculateNFPBatch(
        const Polygon& A,
        const std::vector<Polygon>& Blist,
        bool inner = false
    );

    /**
     * @brief Calculate optimal scale factor for integer conversion
     *
     * Boost.Polygon's Minkowski functions work with integers,
     * so we need to scale floating point coordinates appropriately.
     *
     * @param A First polygon
     * @param B Second polygon
     * @return Scale factor to use
     */
    static double calculateScale(const Polygon& A, const Polygon& B);

private:

    /**
     * @brief Convert polygon to Boost integer polygon
     *
     * @param poly Input polygon
     * @param scale Scale factor
     * @return Boost polygon with integer coordinates
     */
    static boost::polygon::polygon_with_holes_data<int> toBoostIntPolygon(
        const Polygon& poly,
        double scale
    );

    /**
     * @brief Convert Boost integer polygon back to our Polygon
     *
     * @param boostPoly Boost polygon with integer coordinates
     * @param scale Scale factor used for conversion
     * @return Our Polygon with floating point coordinates
     */
    static Polygon fromBoostIntPolygon(
        const boost::polygon::polygon_with_holes_data<int>& boostPoly,
        double scale
    );

    /**
     * @brief Extract polygons from Boost polygon set
     *
     * @param polySet Boost polygon set
     * @param scale Scale factor
     * @return Vector of our Polygons
     */
    static std::vector<Polygon> fromBoostPolygonSet(
        const boost::polygon::polygon_set_data<int>& polySet,
        double scale
    );
};

} // namespace deepnest

#endif // DEEPNEST_MINKOWSKISUM_H
