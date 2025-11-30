#ifndef DEEPNEST_CALCULATENFP_H
#define DEEPNEST_CALCULATENFP_H

#include "../../core/Polygon.h"
#include <vector>

namespace deepnest {

/**
 * @brief Calculate the No-Fit Polygon (NFP) for two polygons using Minkowski sum.
 * 
 * This function is a port of the calculateNFP function from node-calculateNFP.
 * It uses integer coordinates internally for robustness.
 * 
 * @param A The stationary polygon (usually the sheet or already placed part).
 * @param B The orbiting polygon (the part being placed).
 * @return A vector of polygons representing the NFP.
 */
std::vector<Polygon> calculateNFP(const Polygon& A, const Polygon& B);

} // namespace deepnest

#endif // DEEPNEST_CALCULATENFP_H
