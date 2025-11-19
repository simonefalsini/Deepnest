#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include <boost/polygon/polygon.hpp>
#include <limits>
#include <algorithm>
#include <cmath>
#include <iostream>

#undef min
#undef max

namespace deepnest {

using namespace boost::polygon;

// Type aliases for Boost.Polygon types with integer coordinates
using IntPoint = point_data<int>;
using IntPolygon = polygon_data<int>;
using IntPolygonWithHoles = polygon_with_holes_data<int>;
using IntPolygonSet = polygon_set_data<int>;
using IntEdge = std::pair<IntPoint, IntPoint>;

// Forward declarations of helper functions from minkowski.cc
namespace {

void convolve_two_segments(std::vector<IntPoint>& figure,
                          const IntEdge& a,
                          const IntEdge& b) {
    figure.clear();
    figure.push_back(IntPoint(a.first));
    figure.push_back(IntPoint(a.first));
    figure.push_back(IntPoint(a.second));
    figure.push_back(IntPoint(a.second));
    convolve(figure[0], b.second);
    convolve(figure[1], b.first);
    convolve(figure[2], b.first);
    convolve(figure[3], b.second);
}

template <typename itrT1, typename itrT2>
void convolve_two_point_sequences(IntPolygonSet& result,
                                  itrT1 ab, itrT1 ae,
                                  itrT2 bb, itrT2 be) {
    if (ab == ae || bb == be)
        return;

    IntPoint first_a = *ab;
    IntPoint prev_a = *ab;
    std::vector<IntPoint> vec;
    IntPolygon poly;
    ++ab;

    for (; ab != ae; ++ab) {
        IntPoint first_b = *bb;
        IntPoint prev_b = *bb;
        itrT2 tmpb = bb;
        ++tmpb;

        for (; tmpb != be; ++tmpb) {
            convolve_two_segments(vec,
                                std::make_pair(prev_b, *tmpb),
                                std::make_pair(prev_a, *ab));
            set_points(poly, vec.begin(), vec.end());
            result.insert(poly);
            prev_b = *tmpb;
        }
        prev_a = *ab;
    }
}

template <typename itrT>
void convolve_point_sequence_with_polygons(IntPolygonSet& result,
                                          itrT b, itrT e,
                                          const std::vector<IntPolygonWithHoles>& polygons) {
    for (std::size_t i = 0; i < polygons.size(); ++i) {
        convolve_two_point_sequences(result, b, e,
                                    begin_points(polygons[i]),
                                    end_points(polygons[i]));

        for (auto itrh = begin_holes(polygons[i]);
             itrh != end_holes(polygons[i]); ++itrh) {
            convolve_two_point_sequences(result, b, e,
                                        begin_points(*itrh),
                                        end_points(*itrh));
        }
    }
}

void convolve_two_polygon_sets(IntPolygonSet& result,
                              const IntPolygonSet& a,
                              const IntPolygonSet& b) {
    result.clear();
    std::vector<IntPolygonWithHoles> a_polygons;
    std::vector<IntPolygonWithHoles> b_polygons;
    a.get(a_polygons);
    b.get(b_polygons);

    for (std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
        convolve_point_sequence_with_polygons(result,
                                             begin_points(a_polygons[ai]),
                                             end_points(a_polygons[ai]),
                                             b_polygons);

        for (auto itrh = begin_holes(a_polygons[ai]);
             itrh != end_holes(a_polygons[ai]); ++itrh) {
            convolve_point_sequence_with_polygons(result,
                                                 begin_points(*itrh),
                                                 end_points(*itrh),
                                                 b_polygons);
        }

        for (std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
            IntPolygonWithHoles tmp_poly = a_polygons[ai];
            result.insert(convolve(tmp_poly, *(begin_points(b_polygons[bi]))));
            tmp_poly = b_polygons[bi];
            result.insert(convolve(tmp_poly, *(begin_points(a_polygons[ai]))));
        }
    }
}

} // anonymous namespace

// ========== Public Methods ==========

double MinkowskiSum::calculateScale(const Polygon& A, const Polygon& B) {
    // Calculate bounding boxes
    BoundingBox bbA = A.bounds();
    BoundingBox bbB = B.bounds();

    // Find maximum bounds for combined polygon
    double Amaxx = bbA.right();
    double Aminx = bbA.left();
    double Amaxy = bbA.bottom();
    double Aminy = bbA.top();

    double Bmaxx = bbB.right();
    double Bminx = bbB.left();
    double Bmaxy = bbB.bottom();
    double Bminy = bbB.top();

    double Cmaxx = Amaxx + Bmaxx;
    double Cminx = Aminx + Bminx;
    double Cmaxy = Amaxy + Bmaxy;
    double Cminy = Aminy + Bminy;

    double maxxAbs = std::max(Cmaxx, std::fabs(Cminx));
    double maxyAbs = std::max(Cmaxy, std::fabs(Cminy));
    double maxda = std::max(maxxAbs, maxyAbs);

    if (maxda < 1.0) {
        maxda = 1.0;
    }

    // Calculate scale factor
    // Use 0.1 factor as in original code
    int maxi = std::numeric_limits<int>::max();
    double scale = (0.1 * static_cast<double>(maxi)) / maxda;

    return scale;
}

IntPolygonWithHoles MinkowskiSum::toBoostIntPolygon(const Polygon& poly, double scale) {
    // CRITICAL FIX: Aggressive cleaning to prevent "invalid comparator" errors
    // in Boost.Polygon scanline algorithm

    // Convert outer boundary with rounding
    std::vector<IntPoint> points;
    points.reserve(poly.points.size());

    for (const auto& p : poly.points) {
        // Use proper rounding instead of truncation to reduce quantization errors
        int x = static_cast<int>(std::round(scale * p.x));
        int y = static_cast<int>(std::round(scale * p.y));
        points.push_back(IntPoint(x, y));
    }

    // CRITICAL: Aggressive cleaning function
    // Removes: duplicates, near-duplicates, and collinear points

    // TUNABLE PARAMETERS (increase if crashes persist in Debug mode):
    // - MIN_EDGE_DISTANCE_SQ: Minimum edge length squared (default: 1)
    //   Increase to 4 or 9 if crashes continue
    // - COLLINEARITY_THRESHOLD: Cross product threshold (default: 2)
    //   Increase to 5 or 10 for more aggressive simplification
    constexpr int MIN_EDGE_DISTANCE_SQ = 1;      // Minimum: distSq > 1
    constexpr long long COLLINEARITY_THRESHOLD = 2;  // Minimum: |cross| > 2

    auto cleanPoints = [](std::vector<IntPoint>& pts) -> bool {
        if (pts.size() < 3) return false;

        std::vector<IntPoint> cleaned;
        cleaned.reserve(pts.size());

        // STEP 1: Remove exact duplicates and very close points
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t next = (i + 1) % pts.size();

            // Calculate distance squared to next point
            int dx = pts[next].x() - pts[i].x();
            int dy = pts[next].y() - pts[i].y();
            int distSq = dx * dx + dy * dy;

            // Only keep if distance > MIN_EDGE_DISTANCE_SQ
            if (distSq > MIN_EDGE_DISTANCE_SQ) {
                cleaned.push_back(pts[i]);
            }
        }

        if (cleaned.size() < 3) return false;

        // STEP 2: Remove collinear points (aggressive simplification)
        std::vector<IntPoint> final;
        final.reserve(cleaned.size());

        for (size_t i = 0; i < cleaned.size(); ++i) {
            size_t prev = (i == 0) ? cleaned.size() - 1 : i - 1;
            size_t next = (i + 1) % cleaned.size();

            // Vectors
            int dx1 = cleaned[i].x() - cleaned[prev].x();
            int dy1 = cleaned[i].y() - cleaned[prev].y();
            int dx2 = cleaned[next].x() - cleaned[i].x();
            int dy2 = cleaned[next].y() - cleaned[i].y();

            // Cross product to detect collinearity
            // If cross product is 0, points are collinear
            long long cross = static_cast<long long>(dx1) * dy2 -
                             static_cast<long long>(dy1) * dx2;

            // Keep point if NOT collinear (|cross| > COLLINEARITY_THRESHOLD)
            if (std::abs(cross) > COLLINEARITY_THRESHOLD) {
                final.push_back(cleaned[i]);
            }
        }

        if (final.size() >= 3) {
            pts = std::move(final);
            return true;
        }

        return false;
    };

    // Clean and validate
    if (!cleanPoints(points)) {
        // Return empty polygon if cleaning failed
        return IntPolygonWithHoles();
    }

    IntPolygonWithHoles result;

    try {
        set_points(result, points.begin(), points.end());
    }
    catch (...) {
        // If set_points fails, return empty
        return IntPolygonWithHoles();
    }

    // Convert holes with same aggressive cleaning
    if (!poly.children.empty()) {
        std::vector<IntPolygon> holes;
        holes.reserve(poly.children.size());

        for (const auto& hole : poly.children) {
            std::vector<IntPoint> holePoints;
            holePoints.reserve(hole.points.size());

            for (const auto& p : hole.points) {
                int x = static_cast<int>(std::round(scale * p.x));
                int y = static_cast<int>(std::round(scale * p.y));
                holePoints.push_back(IntPoint(x, y));
            }

            // Aggressive cleaning for holes
            if (cleanPoints(holePoints)) {
                try {
                    IntPolygon holePoly;
                    set_points(holePoly, holePoints.begin(), holePoints.end());
                    holes.push_back(holePoly);
                }
                catch (...) {
                    // Skip this hole if it fails
                    continue;
                }
            }
        }

        if (!holes.empty()) {
            try {
                set_holes(result, holes.begin(), holes.end());
            }
            catch (...) {
                // Continue without holes if set_holes fails
            }
        }
    }

    return result;
}

Polygon MinkowskiSum::fromBoostIntPolygon(const IntPolygonWithHoles& boostPoly, double scale) {
    Polygon result;
    double invScale = 1.0 / scale;

    // CRITICAL FIX: Extract points to temporary containers first to avoid
    // dangling iterators and ensure complete data copy before Boost objects are destroyed

    // Extract outer boundary to temporary container
    std::vector<IntPoint> outerPoints;
    for (auto it = begin_points(boostPoly); it != end_points(boostPoly); ++it) {
        outerPoints.push_back(*it);  // Complete copy of point data
    }

    // Convert outer points
    result.points.reserve(outerPoints.size());
    for (const auto& pt : outerPoints) {
        result.points.emplace_back(
            static_cast<double>(pt.x()) * invScale,
            static_cast<double>(pt.y()) * invScale
        );
    }

    // Extract holes - copy data completely before conversion
    std::vector<std::vector<IntPoint>> holePointsList;
    for (auto holeIt = begin_holes(boostPoly); holeIt != end_holes(boostPoly); ++holeIt) {
        std::vector<IntPoint> holePoints;
        for (auto pointIt = begin_points(*holeIt); pointIt != end_points(*holeIt); ++pointIt) {
            holePoints.push_back(*pointIt);  // Complete copy
        }
        holePointsList.push_back(holePoints);
    }

    // Convert holes
    result.children.reserve(holePointsList.size());
    for (const auto& holePoints : holePointsList) {
        Polygon hole;
        hole.points.reserve(holePoints.size());
        for (const auto& pt : holePoints) {
            hole.points.emplace_back(
                static_cast<double>(pt.x()) * invScale,
                static_cast<double>(pt.y()) * invScale
            );
        }
        result.children.push_back(std::move(hole));  // Move instead of copy
    }

    return result;
}

std::vector<Polygon> MinkowskiSum::fromBoostPolygonSet(
    const IntPolygonSet& polySet, double scale) {

    // CRITICAL FIX: Ensure complete copy of data from Boost internal structures
    // before any processing to avoid dangling references after polySet destruction
    std::vector<IntPolygonWithHoles> boostPolygons;

    // CRITICAL: Wrap get() in try-catch to handle "invalid comparator" errors
    // that can occur in Boost.Polygon scanline algorithm with degenerate geometries
    try {
        polySet.get(boostPolygons);  // This triggers clean() which uses scanline algorithm
    }
    catch (const std::exception& e) {
        // If Boost.Polygon scanline fails (invalid comparator, etc.), return empty
        // This can happen with degenerate geometries even after cleaning
        std::cerr << "WARNING: Boost.Polygon get() failed: " << e.what()
                  << " - returning empty NFP (degenerate geometry)" << std::endl;
        return std::vector<Polygon>();
    }
    catch (...) {
        // Catch any other exceptions from Boost.Polygon
        std::cerr << "WARNING: Boost.Polygon get() failed with unknown exception"
                  << " - returning empty NFP (degenerate geometry)" << std::endl;
        return std::vector<Polygon>();
    }

    std::vector<Polygon> result;
    result.reserve(boostPolygons.size());

    // Process all polygons immediately while boostPolygons is still valid
    // fromBoostIntPolygon now does complete data extraction before conversion
    for (const auto& boostPoly : boostPolygons) {
        Polygon converted = fromBoostIntPolygon(boostPoly, scale);
        // Only add non-empty polygons
        if (!converted.points.empty()) {
            result.push_back(std::move(converted));
        }
    }

    // Explicitly clear boost containers to ensure no dangling references
    boostPolygons.clear();

    return result;
}

std::vector<Polygon> MinkowskiSum::calculateNFP(
    const Polygon& A,
    const Polygon& B,
    bool inner) {

    // Validate input
    if (A.points.empty() || B.points.empty()) {
        return std::vector<Polygon>();
    }

    // Calculate scale factor
    double scale = calculateScale(A, B);

    // For NFP placement calculations, we ALWAYS need Minkowski difference: A ⊖ B = A ⊕ (-B)
    // This applies to BOTH:
    // - Inner NFP (sheet boundary): sheet ⊖ part
    // - Outer NFP (part-to-part no-overlap): placedPart ⊖ newPart
    // The 'inner' parameter is only used for caching/semantics, not for computation.
    Polygon B_to_use = B;

    // ALWAYS negate all points of B to get -B for Minkowski difference
    for (auto& point : B_to_use.points) {
        point.x = -point.x;
        point.y = -point.y;
    }
    // CRITICAL: Negating coordinates inverts the winding order!
    // CCW becomes CW. We must reverse the point order to restore CCW winding.
    std::reverse(B_to_use.points.begin(), B_to_use.points.end());

    // Also negate children (holes)
    for (auto& child : B_to_use.children) {
        for (auto& point : child.points) {
            point.x = -point.x;
            point.y = -point.y;
        }
        // Reverse child points to restore CCW winding after negation
        std::reverse(child.points.begin(), child.points.end());
    }

    // CRITICAL FIX: All Boost.Polygon operations are scoped to ensure proper cleanup
    // and immediate data extraction before any Boost objects are destroyed
    std::vector<Polygon> nfps;
    {
        // Convert to Boost integer polygons - scoped lifetime
        IntPolygonSet polySetA, polySetB, result;

        IntPolygonWithHoles boostA = toBoostIntPolygon(A, scale);
        IntPolygonWithHoles boostB = toBoostIntPolygon(B_to_use, scale);

        // Check if conversion produced valid polygons
        if (boostA.begin() == boostA.end() || boostB.begin() == boostB.end()) {
            // Empty polygon after cleaning - return empty NFP
            return std::vector<Polygon>();
        }

        try {
            polySetA.insert(boostA);
            polySetB.insert(boostB);

            // Compute Minkowski sum - this can fail with invalid comparator
            convolve_two_polygon_sets(result, polySetA, polySetB);

            // CRITICAL: Extract and convert data IMMEDIATELY while all Boost objects are still valid
            // fromBoostPolygonSet now does complete deep copy before any Boost object destruction
            nfps = fromBoostPolygonSet(result, scale);

            // Explicitly clear Boost containers to release any internal memory
            result.clear();
            polySetA.clear();
            polySetB.clear();
        }
        catch (const std::exception& e) {
            std::cerr << "WARNING: Boost.Polygon Minkowski convolution failed: " << e.what()
                      << " - returning empty NFP (geometry too complex/degenerate)" << std::endl;
            return std::vector<Polygon>();
        }
        catch (...) {
            std::cerr << "WARNING: Boost.Polygon Minkowski convolution failed with unknown error"
                      << " - returning empty NFP (geometry too complex/degenerate)" << std::endl;
            return std::vector<Polygon>();
        }
    }
    // All Boost objects destroyed here - nfps contains completely independent data

    return nfps;
}

std::vector<std::vector<Polygon>> MinkowskiSum::calculateNFPBatch(
    const Polygon& A,
    const std::vector<Polygon>& Blist,
    bool inner) {

    std::vector<std::vector<Polygon>> results;
    results.reserve(Blist.size());

    // For batch processing, we could optimize by reusing the
    // converted A polygon, but for now keep it simple
    for (const auto& B : Blist) {
        results.push_back(calculateNFP(A, B, inner));
    }

    return results;
}

} // namespace deepnest
