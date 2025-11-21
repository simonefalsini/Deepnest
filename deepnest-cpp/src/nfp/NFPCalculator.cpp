#include "../../include/deepnest/nfp/NFPCalculator.h"
#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include <algorithm>
#include <iostream>

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
        // PHASE 3.2: FALLBACK using Orbital-Based NFP calculation
        // When Minkowski sum fails (empty result), use the orbital tracing algorithm
        // This is more accurate than bounding box approximation
        std::cerr << "WARNING: Minkowski sum failed for polygons A(id=" << A.id
                 << ") and B(id=" << B.id << "), trying orbital tracing fallback..." << std::endl;

        // Use the orbital-based noFitPolygon algorithm
        std::vector<std::vector<Point>> orbitalNFPs = GeometryUtil::noFitPolygon(
            A.points, B.points, inside, false  // searchEdges=false for single NFP
        );

        if (!orbitalNFPs.empty() && !orbitalNFPs[0].empty()) {
            // Convert std::vector<Point> to Polygon
            Polygon fallbackNFP;
            fallbackNFP.points = orbitalNFPs[0];
            fallbackNFP.id = -1;  // Mark as generated
            fallbackNFP.rotation = 0;

            std::cerr << "SUCCESS: Orbital tracing generated NFP with " << fallbackNFP.points.size()
                     << " points" << std::endl;
            return fallbackNFP;
        }

        std::cerr << "ERROR: Both Minkowski and orbital tracing failed!" << std::endl;
        std::cerr << "  Polygon A(id=" << A.id << "): " << A.points.size() << " points" << std::endl;
        std::cerr << "  Polygon B(id=" << B.id << "): " << B.points.size() << " points" << std::endl;

        // LAST RESORT FALLBACK: Use conservative bounding box approximation
        // This ensures we always have SOME collision detection, even if imprecise
        std::cerr << "  LAST RESORT: Using conservative bounding box NFP approximation" << std::endl;

        BoundingBox bboxA = A.bounds();
        BoundingBox bboxB = B.bounds();

        // For OUTSIDE NFP: Create a rectangle around A expanded by B's dimensions
        // This is overly conservative but guarantees no overlap
        Polygon conservativeNFP;
        if (!inside) {
            double x = bboxA.x - bboxB.width;
            double y = bboxA.y - bboxB.height;
            double w = bboxA.width + 2 * bboxB.width;
            double h = bboxA.height + 2 * bboxB.height;

            conservativeNFP.points = {
                {x, y},
                {x + w, y},
                {x + w, y + h},
                {x, y + h}
            };

            std::cerr << "  Generated conservative NFP: " << w << " x " << h << " rectangle" << std::endl;
        } else {
            // For INSIDE NFP: Shrink A's bounding box by B's dimensions
            // This ensures B fits completely inside
            double x = bboxA.x + bboxB.width / 2;
            double y = bboxA.y + bboxB.height / 2;
            double w = std::max(1.0, bboxA.width - bboxB.width);
            double h = std::max(1.0, bboxA.height - bboxB.height);

            if (w > 1.0 && h > 1.0) {
                conservativeNFP.points = {
                    {x, y},
                    {x + w, y},
                    {x + w, y + h},
                    {x, y + h}
                };
                std::cerr << "  Generated conservative inner NFP: " << w << " x " << h << " rectangle" << std::endl;
            } else {
                std::cerr << "  WARNING: B is too large to fit inside A, returning empty NFP" << std::endl;
                return Polygon(); // Truly impossible to fit
            }
        }

        conservativeNFP.id = -999;  // Mark as fallback approximation
        return conservativeNFP;
    }

    // DEBUG LOGGING - DISABLED for cleaner output
    // static bool first_nfp_call = true;
    // if (first_nfp_call && inside) {
    //     std::cout << "\n=== NFPCalculator::computeNFP DEBUG (inside=" << inside << ") ===" << std::endl;
    //     std::cout << "MinkowskiSum returned " << nfps.size() << " NFP(s)" << std::endl;
    //     if (!nfps.empty()) {
    //         std::cout << "NFP[0] first 4 points BEFORE translation: ";
    //         for (size_t i = 0; i < std::min(size_t(4), nfps[0].points.size()); ++i) {
    //             std::cout << "(" << nfps[0].points[i].x << "," << nfps[0].points[i].y << ") ";
    //         }
    //         std::cout << std::endl;
    //         std::cout << "B.points[0] (translation): (" << B.points[0].x << "," << B.points[0].y << ")" << std::endl;
    //     }
    //     std::cout.flush();
    // }

    // If only one result, return it
    if (nfps.size() == 1) {
        Polygon result = nfps[0];

        // Translate by B's first point as in JavaScript code
        // (clipperNfp[i].x += B[0].x; clipperNfp[i].y += B[0].y;)
        if (!B.points.empty()) {
            result = result.translate(B.points[0].x, B.points[0].y);
        }

        // DEBUG LOGGING - DISABLED for cleaner output
        // if (first_nfp_call && inside) {
        //     std::cout << "NFP[0] first 4 points AFTER translation: ";
        //     for (size_t i = 0; i < std::min(size_t(4), result.points.size()); ++i) {
        //         std::cout << "(" << result.points[i].x << "," << result.points[i].y << ") ";
        //     }
        //     std::cout << std::endl;
        //     std::cout << "NFP[0] children count: " << result.children.size() << std::endl;
        //     if (!result.children.empty()) {
        //         std::cout << "NFP[0].children[0] first 4 points: ";
        //         for (size_t i = 0; i < std::min(size_t(4), result.children[0].points.size()); ++i) {
        //             std::cout << "(" << result.children[0].points[i].x << "," << result.children[0].points[i].y << ") ";
        //         }
        //         std::cout << std::endl;
        //     }
        //     std::cout.flush();
        //     first_nfp_call = false;
        // }

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
    std::vector<Polygon> cached;
    if (cache_.find(A.id, B.id, A.rotation, B.rotation, cached, inside)) {
        // Cache hit - return first polygon from cached result
        if (!cached.empty()) {
            return cached.front();
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
    double originalWidth = bounds.width;
    double originalHeight = bounds.height;

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
    std::vector<Polygon> cached;
    if (cache_.find(A.source, B.source, 0.0, B.rotation, cached, true)) {
        return cached;
    }

    // DEBUG LOGGING - DISABLED for cleaner output
    // static bool first_call = true;
    // if (first_call) {
    //     std::cout << "\n=== NFPCalculator::getInnerNFP DEBUG ===" << std::endl;
    //     std::cout << "Sheet (A) first 4 points: ";
    //     for (size_t i = 0; i < std::min(size_t(4), A.points.size()); ++i) {
    //         std::cout << "(" << A.points[i].x << "," << A.points[i].y << ") ";
    //     }
    //     std::cout << std::endl;
    //     std::cout << "Part (B) first 4 points: ";
    //     for (size_t i = 0; i < std::min(size_t(4), B.points.size()); ++i) {
    //         std::cout << "(" << B.points[i].x << "," << B.points[i].y << ") ";
    //     }
    //     std::cout << std::endl;
    //     std::cout << "Part (B) rotation: " << B.rotation << std::endl;
    //     std::cout.flush();
    //     first_call = false;
    // }

    // Create frame around A (background.js line 744)
    Polygon frame = createFrame(A);

    // DEBUG LOGGING - DISABLED for cleaner output
    // static bool first_frame = true;
    // if (first_frame) {
    //     std::cout << "Frame first 4 points: ";
    //     for (size_t i = 0; i < std::min(size_t(4), frame.points.size()); ++i) {
    //         std::cout << "(" << frame.points[i].x << "," << frame.points[i].y << ") ";
    //     }
    //     std::cout << std::endl;
    //     std::cout << "Frame children count: " << frame.children.size() << std::endl;
    //     std::cout.flush();
    //     first_frame = false;
    // }

    // Compute outer NFP between frame and B with inside=true (background.js line 746)
    Polygon frameNfp = getOuterNFP(frame, B, true);

    // CRITICAL FIX: Safety check before accessing children
    // If frameNfp is empty (failed to compute), return immediately
    if (frameNfp.points.empty()) {
        std::cerr << "WARNING: getInnerNFP failed for A(id=" << A.id << ") and B(id=" << B.id << ")" << std::endl;
        std::cerr << "  Frame NFP computation returned empty polygon" << std::endl;
        return {}; // Return empty vector
    }

    // Check if computation succeeded (background.js line 748-750)
    if (frameNfp.children.empty()) {
        std::cerr << "WARNING: getInnerNFP has no children for A(id=" << A.id << ") and B(id=" << B.id << ")" << std::endl;
        return {}; // Return empty vector
    }

    std::vector<Polygon> result;

    // The frame NFP's children contain the actual inner NFP regions
    // In JavaScript, nfp.children contains the valid placement regions
    result = frameNfp.children;

    // DEBUG LOGGING - DISABLED for cleaner output
    // static bool first_result = true;
    // if (first_result) {
    //     std::cout << "FrameNFP points count: " << frameNfp.points.size() << std::endl;
    //     std::cout << "FrameNFP children count: " << frameNfp.children.size() << std::endl;
    //     if (!result.empty()) {
    //         std::cout << "Result[0] (innerNFP) first 4 points: ";
    //         for (size_t i = 0; i < std::min(size_t(4), result[0].points.size()); ++i) {
    //             std::cout << "(" << result[0].points[i].x << "," << result[0].points[i].y << ") ";
    //         }
    //         std::cout << std::endl;
    //     }
    //     std::cout.flush();
    //     first_result = false;
    // }

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
                    // differencePolygons works with point vectors, so we need to convert
                    auto differencePoints = PolygonOperations::differencePolygons(innerPoly.points, holeNfp.points);

                    // Convert back to Polygon objects
                    for (const auto& diffPoints : differencePoints) {
                        Polygon diffPoly;
                        diffPoly.points = diffPoints;
                        updatedResult.push_back(diffPoly);
                    }
                }

                result = updatedResult;
            }
        }
    }

    // Cache the result (using source IDs and rotation)
    if (!result.empty()) {
        cache_.insert(A.source, B.source, 0.0, B.rotation, result, true);
    }

    return result;
}

void NFPCalculator::clearCache() {
    cache_.clear();
}

std::tuple<size_t, size_t, size_t> NFPCalculator::getCacheStats() const {
    return std::make_tuple(cache_.hitCount(), cache_.missCount(), cache_.size());
}

} // namespace deepnest
