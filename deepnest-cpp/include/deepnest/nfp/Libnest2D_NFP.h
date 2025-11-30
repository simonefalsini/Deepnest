#ifndef DEEPNEST_LIBNEST2D_NFP_H
#define DEEPNEST_LIBNEST2D_NFP_H

#include "../core/Polygon.h"
#include <vector>

namespace deepnest {
namespace libnest2d_port {

    /**
     * @brief Calculate NFP for convex polygons using Cuninghame-Green algorithm.
     * Ported from libnest2d.
     * 
     * @param A Stationary polygon (must be convex)
     * @param B Orbiting polygon (must be convex)
     * @return Vector of NFP polygons (usually one)
     */
    std::vector<Polygon> nfpConvexOnly(const Polygon& A, const Polygon& B);

    /**
     * @brief Calculate NFP for simple polygons using Minkowski sum decomposition.
     * Ported from libnest2d.
     * 
     * @param A Stationary polygon
     * @param B Orbiting polygon
     * @return Vector of NFP polygons
     */
    std::vector<Polygon> nfpSimpleSimple(const Polygon& A, const Polygon& B);

    /**
     * @brief Calculate NFP using the sliding algorithm from SVGNest (ported from libnest2d).
#ifndef DEEPNEST_LIBNEST2D_NFP_H
#define DEEPNEST_LIBNEST2D_NFP_H

#include "../core/Polygon.h"
#include <vector>

namespace deepnest {
namespace libnest2d_port {

    /**
     * @brief Calculate NFP for convex polygons using Cuninghame-Green algorithm.
     * Ported from libnest2d.
     * 
     * @param A Stationary polygon (must be convex)
     * @param B Orbiting polygon (must be convex)
     * @return Vector of NFP polygons (usually one)
     */
    std::vector<Polygon> nfpConvexOnly(const Polygon& A, const Polygon& B);

    /**
     * @brief Calculate NFP for simple polygons using Minkowski sum decomposition.
     * Ported from libnest2d.
     * 
     * @param A Stationary polygon
     * @param B Orbiting polygon
     * @return Vector of NFP polygons
     */
    std::vector<Polygon> nfpSimpleSimple(const Polygon& A, const Polygon& B);

    /**
     * @brief Calculate NFP using the sliding algorithm from SVGNest (ported from libnest2d).
     * 
     * @param A Stationary polygon
     * @param B Orbiting polygon
     * @param inside If true, calculate inner NFP (not fully supported in this port yet)
     * @return Vector of NFP polygons
     */
    std::vector<Polygon> svgnest_noFitPolygon(const Polygon& A, const Polygon& B, bool inside = false);

    // Port of libnest2d/tools/libnfporb/libnfporb.hpp generateNFP
    std::vector<Polygon> libnfporb_generateNFP(const Polygon& A, const Polygon& B);

} // namespace libnest2d_port
} // namespace deepnest

#endif // DEEPNEST_LIBNEST2D_NFP_H
