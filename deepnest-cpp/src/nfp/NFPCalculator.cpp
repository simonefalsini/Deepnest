#include "../../include/deepnest/nfp/NFPCalculator.h"
#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include <algorithm>

namespace deepnest {

NFPCalculator::NFPCalculator(NFPCache& cache)
    : cache_(cache) {
}

Polygon NFPCalculator::computeNFP(const Polygon& A, const Polygon& B, bool inside) const {
    // Use MinkowskiSum to calculate NFP
    // The MinkowskiSum::calculateNFP returns a vector of polygons,
    // we need to select the largest one (by area) as per JavaScript implementation
    auto nfps = MinkowskiSum::calculateNFP(A, B, inside);

    if (nfps.empty()) {
        return Polygon(); // Return empty polygon
    }

    // If only one result, return it
    if (nfps.size() == 1) {
        Polygon result = nfps[0];

        // Translate by B's first point as in JavaScript code
        // (clipperNfp[i].x += B[0].x; clipperNfp[i].y += B[0].y;)
        if (!B.points.empty()) {
            result = result.translate(B.points[0].x, B.points[0].y);
        }

        return result;
    }

    // Select the polygon with largest area (as in JavaScript: largestArea)
    Polygon largestNFP;
    double largestArea = 0.0;

    for (const auto& nfp : nfps) {
        double area = std::abs(GeometryUtil::polygonArea(nfp.points));
        if (area > largestArea) {
            largestArea = area;
            largestNFP = nfp;
        }
    }

    // Translate by B's first point
    if (!B.points.empty()) {
        largestNFP = largestNFP.translate(B.points[0].x, B.points[0].y);
    }

    return largestNFP;
}

Polygon NFPCalculator::getOuterNFP(const Polygon& A, const Polygon& B, bool inside) {
    // Try cache lookup first (background.js line 636-640)
    auto cached = cache_.get(A.id, B.id, A.rotation, B.rotation, inside);
    if (cached.has_value()) {
        // Cache hit - return first polygon from cached result
        if (!cached->empty()) {
            return cached->front();
        }
    }

    // Not found in cache - compute NFP (background.js line 643-684)
    Polygon nfp = computeNFP(A, B, inside);

    if (nfp.points.empty()) {
        return Polygon(); // Failed to compute
    }

    // Store in cache if not computing inner NFP (background.js line 697-707)
    // Only cache outer NFPs (when inside=false)
    if (!inside) {
        std::vector<Polygon> cacheEntry = {nfp};
        cache_.insert(A.id, B.id, A.rotation, B.rotation, cacheEntry, inside);
    }

    return nfp;
}

Polygon NFPCalculator::createFrame(const Polygon& A) const {
    // Get bounding box (background.js line 713)
    BoundingBox bounds = A.bounds();

    // Expand bounds by 10% (background.js line 716-719)
    double originalWidth = bounds.width();
    double originalHeight = bounds.height();

    double expandedWidth = originalWidth * 1.1;
    double expandedHeight = originalHeight * 1.1;

    double newX = bounds.left() - 0.5 * (expandedWidth - originalWidth);
    double newY = bounds.top() - 0.5 * (expandedHeight - originalHeight);

    // Create rectangular frame (background.js line 721-725)
    Polygon frame;
    frame.points.push_back(Point(newX, newY));
    frame.points.push_back(Point(newX + expandedWidth, newY));
    frame.points.push_back(Point(newX + expandedWidth, newY + expandedHeight));
    frame.points.push_back(Point(newX, newY + expandedHeight));

    // Add A as a child (hole) of the frame (background.js line 727)
    frame.children.push_back(A);

    // Copy metadata (background.js line 728-729)
    frame.source = A.source;
    frame.rotation = 0;

    return frame;
}

Polygon NFPCalculator::getFrame(const Polygon& A) const {
    return createFrame(A);
}

std::vector<Polygon> NFPCalculator::getInnerNFP(const Polygon& A, const Polygon& B) {
    // Try cache lookup first (background.js line 735-742)
    // For inner NFP, rotation of A is always 0
    auto cached = cache_.get(A.source, B.source, 0.0, B.rotation, true);
    if (cached.has_value()) {
        return *cached;
    }

    // Create frame around A (background.js line 744)
    Polygon frame = createFrame(A);

    // Compute outer NFP between frame and B with inside=true (background.js line 746)
    Polygon frameNfp = getOuterNFP(frame, B, true);

    // Check if computation succeeded (background.js line 748-750)
    if (frameNfp.points.empty() || frameNfp.children.empty()) {
        return {}; // Return empty vector
    }

    std::vector<Polygon> result;

    // The frame NFP's children contain the actual inner NFP regions
    // In JavaScript, nfp.children contains the valid placement regions
    result = frameNfp.children;

    // Handle holes in A (background.js line 753-754)
    if (!A.children.empty()) {
        // For each hole in A, compute its NFP with B
        for (const auto& hole : A.children) {
            Polygon holeNfp = getOuterNFP(hole, B, false);

            if (!holeNfp.points.empty()) {
                // Subtract hole NFP from result using Clipper2
                // This represents forbidden regions where B would overlap with A's holes
                std::vector<Polygon> updatedResult;

                for (auto& innerPoly : result) {
                    // Use PolygonOperations to perform difference
                    auto difference = PolygonOperations::difference(innerPoly, holeNfp);
                    updatedResult.insert(updatedResult.end(), difference.begin(), difference.end());
                }

                result = updatedResult;
            }
        }
    }

    // Cache the result (using source IDs and rotation)
    if (result.empty() == false) {
        cache_.insert(A.source, B.source, 0.0, B.rotation, result, true);
    }

    return result;
}

void NFPCalculator::clearCache() {
    cache_.clear();
}

std::tuple<size_t, size_t, size_t> NFPCalculator::getCacheStats() const {
    return cache_.getStats();
}

} // namespace deepnest
