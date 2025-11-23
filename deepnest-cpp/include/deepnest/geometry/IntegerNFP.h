/**
 * @file IntegerNFP.h
 * @brief Integer-based No-Fit Polygon (Orbital Tracing) implementation
 *
 * This module provides a robust NFP calculation using exact integer arithmetic.
 * It serves as a FALLBACK when the MinkowskiSum-based NFP calculation fails.
 *
 * DESIGN:
 * - Uses int64_t coordinates scaled by 10000 (4 decimal places precision)
 * - Exact geometric predicates (no floating-point tolerance issues)
 * - Orbital tracing algorithm following computational geometry standards
 *
 * USAGE:
 * This is called automatically by GeometryUtil::noFitPolygon() when
 * MinkowskiSum::calculateNFP() returns an empty result.
 */

#ifndef DEEPNEST_INTEGER_NFP_H
#define DEEPNEST_INTEGER_NFP_H

#include "../core/Point.h"
#include <vector>

namespace deepnest {
namespace IntegerNFP {

/**
 * @brief Compute No-Fit Polygon using integer-based orbital tracing
 *
 * This is the FALLBACK implementation when MinkowskiSum fails.
 * Uses exact integer arithmetic to avoid floating-point precision issues.
 *
 * @param A Stationary polygon
 * @param B Moving polygon
 * @param inside true for INSIDE NFP (B inside A), false for OUTSIDE NFP (B outside A)
 * @return Vector of NFP polygons (usually 1, empty if failed)
 */
std::vector<std::vector<Point>> computeNFP(
    const std::vector<Point>& A,
    const std::vector<Point>& B,
    bool inside
);

} // namespace IntegerNFP
} // namespace deepnest

#endif // DEEPNEST_INTEGER_NFP_H
