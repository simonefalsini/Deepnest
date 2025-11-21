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

    // CRITICAL: During Minkowski convolution, intermediate values can be MUCH larger
    // than the final result. The convolution creates combinations of segments that
    // can produce very large intermediate coordinates before simplification.
    //
    // Instead of just looking at C = A + B, we need to consider the WORST CASE
    // intermediate values during convolution operations.
    //
    // Conservative approach: Use the sum of absolute ranges of both polygons
    double Awidth = std::max(std::fabs(Amaxx), std::fabs(Aminx));
    double Aheight = std::max(std::fabs(Amaxy), std::fabs(Aminy));
    double Bwidth = std::max(std::fabs(Bmaxx), std::fabs(Bminx));
    double Bheight = std::max(std::fabs(Bmaxy), std::fabs(Bminy));

    // Maximum possible intermediate coordinate during convolution
    // This accounts for operations like segment crossings, point combinations, etc.
    double maxda = std::max({
        Awidth + Bwidth,   // X dimension worst case
        Aheight + Bheight, // Y dimension worst case
        std::fabs(Aminx) + std::fabs(Bminx),
        std::fabs(Amaxx) + std::fabs(Bmaxx),
        std::fabs(Aminy) + std::fabs(Bminy),
        std::fabs(Amaxy) + std::fabs(Bmaxy)
    });

    if (maxda < 1.0) {
        maxda = 1.0;
    }

    // Calculate scale factor with VERY conservative margin
    // Use 0.02 (2%) instead of 0.1 (10%) to leave 98% headroom for intermediate values
    // This prevents "invalid comparator" exceptions during polySet.get()
    int maxi = std::numeric_limits<int>::max();
    double scale = (0.02 * static_cast<double>(maxi)) / maxda;

    std::cerr << "[MINK-SCALE] maxda=" << maxda << " scale=" << scale << " (factor=0.02 ultra-conservative)" << std::endl;
    std::cerr << "[MINK-SCALE] A bounds: [" << Aminx << "," << Amaxx << "] x [" << Aminy << "," << Amaxy << "]" << std::endl;
    std::cerr << "[MINK-SCALE] B bounds: [" << Bminx << "," << Bmaxx << "] x [" << Bminy << "," << Bmaxy << "]" << std::endl;

    return scale;
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

    try {
        std::cerr << "[MINK-EXTRACT] Attempting to extract polygons from polygon set..." << std::endl;
        polySet.get(boostPolygons);
        std::cerr << "[MINK-EXTRACT] Successfully extracted " << boostPolygons.size() << " polygon(s)" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[MINK-ERROR] Exception in polySet.get(): " << e.what() << std::endl;
        std::cerr << "[MINK-ERROR] This likely indicates integer overflow in Boost.Polygon" << std::endl;
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

    // RETRY MECHANISM: Try with progressively smaller scale factors if overflow occurs
    // Scale factors to try: 0.02, 0.01, 0.005, 0.002, 0.001
    std::vector<double> scaleFactors = {0.02, 0.01, 0.005, 0.002, 0.001};

    for (size_t attempt = 0; attempt < scaleFactors.size(); ++attempt) {
        double scaleFactor = scaleFactors[attempt];

        try {
            // Calculate scale with current factor
            double scale = calculateScale(A, B);
            // Override with retry factor if not first attempt
            if (attempt > 0) {
                BoundingBox bbA = A.bounds();
                BoundingBox bbB = B_to_use.bounds();
                double Awidth = std::max(std::fabs(bbA.right()), std::fabs(bbA.left()));
                double Aheight = std::max(std::fabs(bbA.bottom()), std::fabs(bbA.top()));
                double Bwidth = std::max(std::fabs(bbB.right()), std::fabs(bbB.left()));
                double Bheight = std::max(std::fabs(bbB.bottom()), std::fabs(bbB.top()));
                double maxda = std::max({Awidth + Bwidth, Aheight + Bheight});
                if (maxda < 1.0) maxda = 1.0;
                scale = (scaleFactor * static_cast<double>(std::numeric_limits<int>::max())) / maxda;
                std::cerr << "[MINK-RETRY] Attempt " << (attempt+1) << " with factor=" << scaleFactor
                         << " scale=" << scale << std::endl;
            }

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

            if (attempt > 0) {
                std::cerr << "[MINK-RETRY] SUCCESS on attempt " << (attempt+1) << std::endl;
            }

            return nfps;

        } catch (const std::exception& e) {
            std::cerr << "[MINK-RETRY] Attempt " << (attempt+1) << " FAILED: " << e.what() << std::endl;
            if (attempt == scaleFactors.size() - 1) {
                std::cerr << "[MINK-RETRY] All retry attempts exhausted. Giving up." << std::endl;
                throw;
            }
            // Continue to next attempt with smaller scale
        }
    }

    // Should never reach here
    return std::vector<Polygon>();
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
