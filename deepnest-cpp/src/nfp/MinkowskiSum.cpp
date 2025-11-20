#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
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
// CRITICAL: These MUST match minkowski.cc exactly!
// minkowski.cc line 14-17:
//   typedef boost::polygon::point_data<int> point;
//   typedef boost::polygon::polygon_set_data<int> polygon_set;
//   typedef boost::polygon::polygon_with_holes_data<int> polygon;
using IntPoint = point_data<int>;
using IntPolygonSet = polygon_set_data<int>;
using IntPolygon = polygon_with_holes_data<int>;  // CRITICAL: with_holes, not polygon_data!
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

IntPolygon MinkowskiSum::toBoostIntPolygon(const Polygon& poly, double scale) {
    // CRITICAL: Extra simplification step immediately before conversion to prevent
    // "invalid comparator" crashes in Boost.Polygon convolution result.
    // Even though polygon was cleaned in Polygon.cpp, we need additional simplification
    // here to remove very close points that can cause problems in the convolution.

    // Use Clipper2 simplifyPolygon to remove points that are too close together
    // Distance of 0.001 removes near-duplicates without losing shape
    std::vector<Point> simplifiedPoints = PolygonOperations::simplifyPolygon(
        poly.points,
        0.001  // Very small distance to remove only near-duplicates
    );

    if (simplifiedPoints.size() < 3) {
        return IntPolygon();
    }

    // Convert outer boundary with rounding for precision
    std::vector<IntPoint> points;
    points.reserve(simplifiedPoints.size());

    for (const auto& p : simplifiedPoints) {
        // Use rounding instead of truncation to reduce quantization errors
        int x = static_cast<int>(std::round(scale * p.x));
        int y = static_cast<int>(std::round(scale * p.y));
        points.push_back(IntPoint(x, y));
    }

    // Validate minimum points
    if (points.size() < 3) {
        return IntPolygon();
    }

    IntPolygon result;

    try {
        set_points(result, points.begin(), points.end());
    }
    catch (...) {
        // If set_points fails, return empty
        return IntPolygon();
    }

    // Convert holes (if any) with same simplification
    if (!poly.children.empty()) {
        std::vector<IntPolygon> holes;
        holes.reserve(poly.children.size());

        for (const auto& hole : poly.children) {
            // Simplify hole before conversion
            std::vector<Point> simplifiedHole = PolygonOperations::simplifyPolygon(
                hole.points,
                0.001  // Same distance as outer boundary
            );

            if (simplifiedHole.size() < 3) {
                continue;  // Skip invalid holes
            }

            std::vector<IntPoint> holePoints;
            holePoints.reserve(simplifiedHole.size());

            for (const auto& p : simplifiedHole) {
                int x = static_cast<int>(std::round(scale * p.x));
                int y = static_cast<int>(std::round(scale * p.y));
                holePoints.push_back(IntPoint(x, y));
            }

            if (holePoints.size() >= 3) {
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
    //
    // NOTE: The problem is that polySet.get() internally calls clean() which uses
    // the scanline algorithm. The scanline comparator can ASSERT with "invalid comparator"
    // when it encounters near-collinear edges or numerical precision issues.
    //
    // We prevent this by:
    // 1. Cleaning inputs in Polygon.cpp (cleanPolygon → simplify → cleanPolygon)
    // 2. Extra simplification in toBoostIntPolygon() BEFORE convolution (distance=0.001)
    // 3. Using proper rounding during conversion to reduce quantization errors
    //
    // This defensive error handling catches any remaining edge cases.
    try {
        // DEBUG: Log before calling get() to help diagnose crashes
        // This helps identify if crash is in get() or in later processing
        #ifdef DEBUG_MINKOWSKI
        std::cerr << "DEBUG: Calling polySet.get() to extract "
                  << polySet.size() << " polygons from result set..." << std::endl;
        #endif

        polySet.get(boostPolygons);  // This triggers clean() which uses scanline algorithm

        #ifdef DEBUG_MINKOWSKI
        std::cerr << "DEBUG: polySet.get() succeeded, extracted "
                  << boostPolygons.size() << " polygon(s)" << std::endl;
        #endif
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

    // CRITICAL: This is a COMPLETE REWRITE following minkowski.cc EXACTLY
    // The key differences from previous attempts:
    // 1. Use operator+= instead of insert()
    // 2. Use simple truncation casting (int) not std::round()
    // 3. NO extra simplification - polygon should already be cleaned
    // 4. Build polygon_set directly, not via IntPolygonWithHoles

    // Calculate scale factor (same as minkowski.cc lines 120-163)
    double scale = calculateScale(A, B);

    // Create Boost.Polygon data structures
    IntPolygonSet a, b, c;
    std::vector<IntPolygon> polys;
    std::vector<IntPoint> pts;

    // Convert A to Boost polygon (minkowski.cc lines 166-178)
    // CRITICAL: Use simple truncation casting like original!
    pts.reserve(A.points.size());
    for (const auto& p : A.points) {
        int x = static_cast<int>(scale * p.x);  // truncation, not rounding!
        int y = static_cast<int>(scale * p.y);
        pts.push_back(IntPoint(x, y));
    }

    IntPolygon poly;
    try {
        set_points(poly, pts.begin(), pts.end());
        a += poly;  // CRITICAL: Use operator+= not insert()!
    }
    catch (...) {
        return std::vector<Polygon>();
    }

    // Subtract holes from a (minkowski.cc lines 180-196)
    for (const auto& hole : A.children) {
        pts.clear();
        pts.reserve(hole.points.size());
        for (const auto& p : hole.points) {
            int x = static_cast<int>(scale * p.x);
            int y = static_cast<int>(scale * p.y);
            pts.push_back(IntPoint(x, y));
        }
        try {
            set_points(poly, pts.begin(), pts.end());
            a -= poly;  // CRITICAL: Use operator-= not difference()!
        }
        catch (...) {
            continue;  // Skip problematic holes
        }
    }

    // For NFP, we ALWAYS need Minkowski difference: A ⊖ B = A ⊕ (-B)
    // Negate and convert B (minkowski.cc lines 198-219)
    pts.clear();
    pts.reserve(B.points.size());

    // Track first point for offset (minkowski.cc lines 203-215)
    double xshift = 0;
    double yshift = 0;

    for (size_t i = 0; i < B.points.size(); i++) {
        const auto& p = B.points[i];
        // CRITICAL: Negate BEFORE scaling, then cast to int with truncation
        int x = -static_cast<int>(scale * p.x);
        int y = -static_cast<int>(scale * p.y);
        pts.push_back(IntPoint(x, y));

        if (i == 0) {
            xshift = p.x;
            yshift = p.y;
        }
    }

    try {
        set_points(poly, pts.begin(), pts.end());
        b += poly;  // CRITICAL: Use operator+= not insert()!
    }
    catch (...) {
        return std::vector<Polygon>();
    }

    // Compute Minkowski sum (minkowski.cc line 223)
    polys.clear();
    try {
        convolve_two_polygon_sets(c, a, b);
        c.get(polys);  // This is where crash can happen with bad geometry
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Boost.Polygon convolution failed: " << e.what() << std::endl;
        return std::vector<Polygon>();
    }
    catch (...) {
        std::cerr << "ERROR: Boost.Polygon convolution failed with unknown error" << std::endl;
        return std::vector<Polygon>();
    }

    // Convert result back to our Polygon format (minkowski.cc lines 228-263)
    std::vector<Polygon> result;
    result.reserve(polys.size());

    double invScale = 1.0 / scale;

    for (const auto& boostPoly : polys) {
        Polygon nfp;

        // Convert outer boundary
        for (auto itr = boostPoly.begin(); itr != boostPoly.end(); ++itr) {
            double x = static_cast<double>((*itr).x()) * invScale + xshift;
            double y = static_cast<double>((*itr).y()) * invScale + yshift;
            nfp.points.emplace_back(x, y);
        }

        // Convert holes
        for (auto holeItr = begin_holes(boostPoly); holeItr != end_holes(boostPoly); ++holeItr) {
            Polygon hole;
            for (auto pointItr = (*holeItr).begin(); pointItr != (*holeItr).end(); ++pointItr) {
                double x = static_cast<double>((*pointItr).x()) * invScale + xshift;
                double y = static_cast<double>((*pointItr).y()) * invScale + yshift;
                hole.points.emplace_back(x, y);
            }
            if (hole.points.size() >= 3) {
                nfp.children.push_back(std::move(hole));
            }
        }

        if (nfp.points.size() >= 3) {
            result.push_back(std::move(nfp));
        }
    }

    return result;
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
