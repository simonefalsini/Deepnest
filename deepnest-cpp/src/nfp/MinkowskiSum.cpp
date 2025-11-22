#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include <boost/polygon/polygon.hpp>
#include <limits>
#include <algorithm>

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
    // SIMPLIFIED APPROACH: Polygons are already scaled to appropriate range
    // Just use direct integer truncation without additional scaling
    //
    // The previous approach of scaling by (factor * INT_MAX / maxda) was
    // counterproductive - it made large coordinates even larger, causing overflow.
    //
    // Simply truncate double coordinates to int - this works if input polygons
    // are already in reasonable coordinate ranges (which they should be).

    std::cerr << "[MINK-SCALE] Using direct truncation (scale=1.0) - no multiplication" << std::endl;

    // Calculate bounds just for logging/verification
    BoundingBox bbA = A.bounds();
    BoundingBox bbB = B.bounds();

    std::cerr << "[MINK-SCALE] A bounds: [" << bbA.left() << "," << bbA.right()
             << "] x [" << bbA.top() << "," << bbA.bottom() << "]" << std::endl;
    std::cerr << "[MINK-SCALE] B bounds: [" << bbB.left() << "," << bbB.right()
             << "] x [" << bbB.top() << "," << bbB.bottom() << "]" << std::endl;

    // Check if coordinates are within safe int range
    double maxCoord = std::max({
        std::fabs(bbA.left()), std::fabs(bbA.right()),
        std::fabs(bbA.top()), std::fabs(bbA.bottom()),
        std::fabs(bbB.left()), std::fabs(bbB.right()),
        std::fabs(bbB.top()), std::fabs(bbB.bottom())
    });

    int intMax = std::numeric_limits<int>::max();
    if (maxCoord > static_cast<double>(intMax) * 0.5) {
        std::cerr << "[MINK-WARNING] Coordinates are large (max=" << maxCoord
                 << "), may exceed int range during convolution!" << std::endl;
        std::cerr << "[MINK-WARNING] Consider pre-scaling input polygons to smaller range" << std::endl;
    }

    return 1.0;  // No scaling - direct truncation
}

IntPolygonWithHoles MinkowskiSum::toBoostIntPolygon(const Polygon& poly, double scale) {
    // Convert outer boundary
    std::vector<IntPoint> points;
    points.reserve(poly.points.size());

    int maxi = std::numeric_limits<int>::max();
    int mini = std::numeric_limits<int>::min();

    for (const auto& p : poly.points) {
        double scaledX = scale * p.x;
        double scaledY = scale * p.y;

        // Check for overflow BEFORE casting and CLAMP to safe range
        if (scaledX > static_cast<double>(maxi)) {
            std::cerr << "[MINK-OVERFLOW] X overflow: p.x=" << p.x << " scale=" << scale
                     << " scaled=" << scaledX << " max=" << maxi << " -> CLAMPING" << std::endl;
            scaledX = static_cast<double>(maxi);
        }
        if (scaledX < static_cast<double>(mini)) {
            std::cerr << "[MINK-OVERFLOW] X underflow: p.x=" << p.x << " scale=" << scale
                     << " scaled=" << scaledX << " min=" << mini << " -> CLAMPING" << std::endl;
            scaledX = static_cast<double>(mini);
        }
        if (scaledY > static_cast<double>(maxi)) {
            std::cerr << "[MINK-OVERFLOW] Y overflow: p.y=" << p.y << " scale=" << scale
                     << " scaled=" << scaledY << " max=" << maxi << " -> CLAMPING" << std::endl;
            scaledY = static_cast<double>(maxi);
        }
        if (scaledY < static_cast<double>(mini)) {
            std::cerr << "[MINK-OVERFLOW] Y underflow: p.y=" << p.y << " scale=" << scale
                     << " scaled=" << scaledY << " min=" << mini << " -> CLAMPING" << std::endl;
            scaledY = static_cast<double>(mini);
        }

        int x = static_cast<int>(scaledX);
        int y = static_cast<int>(scaledY);
        points.push_back(IntPoint(x, y));
    }

    IntPolygonWithHoles result;
        set_points(result, points.begin(), points.end());

    // Convert holes
    if (!poly.children.empty()) {
        std::vector<IntPolygon> holes;
        holes.reserve(poly.children.size());

        for (const auto& hole : poly.children) {
            std::vector<IntPoint> holePoints;
            holePoints.reserve(hole.points.size());

            for (const auto& p : hole.points) {
                int x = static_cast<int>(scale * p.x);
                int y = static_cast<int>(scale * p.y);
                holePoints.push_back(IntPoint(x, y));
            }

                    IntPolygon holePoly;
                    set_points(holePoly, holePoints.begin(), holePoints.end());
                    holes.push_back(holePoly);
                }

                set_holes(result, holes.begin(), holes.end());
            }

    return result;
}

Polygon MinkowskiSum::fromBoostIntPolygon(const IntPolygonWithHoles& boostPoly, double scale) {
    Polygon result;
    double invScale = 1.0 / scale;

    // Extract outer boundary
    for (auto it = begin_points(boostPoly); it != end_points(boostPoly); ++it) {
        result.points.emplace_back(
            static_cast<double>(it->x()) * invScale,
            static_cast<double>(it->y()) * invScale
        );
    }

    // Extract holes
    for (auto holeIt = begin_holes(boostPoly); holeIt != end_holes(boostPoly); ++holeIt) {
        Polygon hole;
        for (auto pointIt = begin_points(*holeIt); pointIt != end_points(*holeIt); ++pointIt) {
            hole.points.emplace_back(
                static_cast<double>(pointIt->x()) * invScale,
                static_cast<double>(pointIt->y()) * invScale
            );
        }
        result.children.push_back(hole);
    }

    return result;
}

std::vector<Polygon> MinkowskiSum::fromBoostPolygonSet(
    const IntPolygonSet& polySet, double scale) {

    std::vector<IntPolygonWithHoles> boostPolygons;

    // First, try to get polygon count without extracting (if possible)
    std::cerr << "[MINK-EXTRACT] Attempting to extract polygons from polygon set..." << std::endl;
    std::cerr << "[MINK-EXTRACT] Scale factor: " << scale << std::endl;

    try {
        polySet.get(boostPolygons);
        std::cerr << "[MINK-EXTRACT] Successfully extracted " << boostPolygons.size() << " polygon(s)" << std::endl;

        // Log point count for each polygon
        for (size_t i = 0; i < boostPolygons.size(); ++i) {
            size_t pointCount = 0;
            for (auto it = begin_points(boostPolygons[i]); it != end_points(boostPolygons[i]); ++it) {
                pointCount++;
            }
            std::cerr << "[MINK-EXTRACT] Polygon[" << i << "]: " << pointCount << " points" << std::endl;
            if (pointCount > 200) {
                std::cerr << "[MINK-WARNING] Polygon has excessive point count (" << pointCount
                         << "), may indicate overflow or degenerate geometry" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[MINK-ERROR] std::exception in polySet.get(): " << e.what() << std::endl;
        std::cerr << "[MINK-ERROR] This likely indicates integer overflow in Boost.Polygon" << std::endl;
        std::cerr << "[MINK-ERROR] Scale factor was: " << scale << std::endl;
        throw;
    } catch (...) {
        std::cerr << "[MINK-ERROR] UNKNOWN exception in polySet.get() (not std::exception)" << std::endl;
        std::cerr << "[MINK-ERROR] This is likely an overflow in Boost.Polygon comparator" << std::endl;
        std::cerr << "[MINK-ERROR] Scale factor was: " << scale << std::endl;
        throw;
    }

    std::vector<Polygon> result;
    result.reserve(boostPolygons.size());

    for (const auto& boostPoly : boostPolygons) {
        result.push_back(fromBoostIntPolygon(boostPoly, scale));
        }

    return result;
}

std::vector<Polygon> MinkowskiSum::calculateNFP(
    const Polygon& A,
    const Polygon& B,
    bool inner) {

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
    // Negating coordinates inverts the winding order!
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

    // Use direct truncation (scale=1.0) - no multiplication
    double scale = calculateScale(A, B);

    // Convert to Boost integer polygons
    IntPolygonSet polySetA, polySetB, result;

    IntPolygonWithHoles boostA = toBoostIntPolygon(A, scale);
    IntPolygonWithHoles boostB = toBoostIntPolygon(B_to_use, scale);

    polySetA.insert(boostA);
    polySetB.insert(boostB);

    // Compute Minkowski sum
    convolve_two_polygon_sets(result, polySetA, polySetB);

    // Convert back to our Polygon type
    std::vector<Polygon> nfps = fromBoostPolygonSet(result, scale);

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
