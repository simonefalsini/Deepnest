#include "../../include/deepnest/nfp/MinkowskiSum.h"
#include "../../include/deepnest/DebugConfig.h"
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

IntPolygonWithHoles MinkowskiSum::toBoostIntPolygon(const Polygon& poly) {
    // Convert outer boundary - direct truncation to int
    std::vector<IntPoint> points;
    points.reserve(poly.points.size());

    for (const auto& p : poly.points) {
        int x = static_cast<int>(p.x);
        int y = static_cast<int>(p.y);
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
                int x = static_cast<int>(p.x);
                int y = static_cast<int>(p.y);
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

Polygon MinkowskiSum::fromBoostIntPolygon(const IntPolygonWithHoles& boostPoly) {
    Polygon result;

    // Convert outer boundary - direct cast to double
    for (auto it = begin_points(boostPoly); it != end_points(boostPoly); ++it) {
        result.points.push_back(Point(
            static_cast<double>(it->x()),
            static_cast<double>(it->y())
        ));
    }

    // Convert holes
    for (auto itrh = begin_holes(boostPoly); itrh != end_holes(boostPoly); ++itrh) {
        Polygon hole;
        for (auto it = begin_points(*itrh); it != end_points(*itrh); ++it) {
            hole.points.push_back(Point(
                static_cast<double>(it->x()),
                static_cast<double>(it->y())
            ));
        }
        result.children.push_back(hole);
    }

    return result;
}

std::vector<Polygon> MinkowskiSum::fromBoostPolygonSet(const IntPolygonSet& polySet) {
    std::vector<IntPolygonWithHoles> boostPolygons;
    polySet.get(boostPolygons);

    LOG_NFP("Extracted " << boostPolygons.size() << " polygon(s) from Minkowski result");

    std::vector<Polygon> result;
    result.reserve(boostPolygons.size());

    for (const auto& boostPoly : boostPolygons) {
        result.push_back(fromBoostIntPolygon(boostPoly));
    }

    return result;
}

std::vector<Polygon> MinkowskiSum::calculateNFP(
    const Polygon& A,
    const Polygon& B) {

    LOG_NFP("Calculating Minkowski NFP: A(" << A.points.size() << " pts) vs B("
           << B.points.size() << " pts), mode=" << (inner ? "INNER" : "OUTER"));

    // For NFP placement calculations, we ALWAYS need Minkowski difference: A ⊖ B = A ⊕ (-B)
    Polygon B_to_use = B;

    // Negate all points of B to get -B for Minkowski difference
    for (auto& point : B_to_use.points) {
        point.x = -point.x;
        point.y = -point.y;
    }
    // Negating coordinates inverts winding order - reverse to restore CCW
    std::reverse(B_to_use.points.begin(), B_to_use.points.end());

    // Also negate children (holes)
    for (auto& child : B_to_use.children) {
        for (auto& point : child.points) {
            point.x = -point.x;
            point.y = -point.y;
        }
        std::reverse(child.points.begin(), child.points.end());
    }

    // Convert to Boost integer polygons (direct truncation)
    IntPolygonSet polySetA, polySetB, result;

    IntPolygonWithHoles boostA = toBoostIntPolygon(A);
    IntPolygonWithHoles boostB = toBoostIntPolygon(B_to_use);

    polySetA.insert(boostA);
    polySetB.insert(boostB);

    // Compute Minkowski sum
    convolve_two_polygon_sets(result, polySetA, polySetB);

    // Convert back to our Polygon type
    std::vector<Polygon> nfps = fromBoostPolygonSet(result);

    LOG_NFP("Minkowski NFP calculation complete: " << nfps.size() << " NFP(s) generated");

    return nfps;
}

std::vector<std::vector<Polygon>> MinkowskiSum::calculateNFPBatch(
    const Polygon& A,
    const std::vector<Polygon>& Blist) {

    std::vector<std::vector<Polygon>> results;
    results.reserve(Blist.size());

    for (const auto& B : Blist) {
        results.push_back(calculateNFP(A, B));
    }

    return results;
}

} // namespace deepnest
