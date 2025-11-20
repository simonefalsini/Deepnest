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
    // CRITICAL: Follow minkowski.cc exactly - use TRUNCATION and MINIMAL cleaning
    // The original implementation uses (int) cast (truncation) and NO cleaning.
    // We apply ONLY consecutive duplicate removal to match original behavior.

    // Convert outer boundary with TRUNCATION (not rounding)
    std::vector<IntPoint> points;
    points.reserve(poly.points.size());

    for (const auto& p : poly.points) {
        // CRITICAL: Use TRUNCATION (not rounding) to match minkowski.cc behavior
        // Original: int x = (int)(inputscale * (double)obj->Get("x")->NumberValue());
        int x = static_cast<int>(scale * p.x);  // Truncation toward zero
        int y = static_cast<int>(scale * p.y);  // Truncation toward zero
        points.push_back(IntPoint(x, y));
    }

    // MINIMAL CLEANING: Remove only consecutive exact duplicate points
    // This matches minkowski.cc behavior (which does NO cleaning at all)
    // We do minimal cleaning only to prevent obvious issues
    auto removeConsecutiveDuplicates = [](std::vector<IntPoint>& pts) {
        if (pts.size() < 2) return;

        std::vector<IntPoint> cleaned;
        cleaned.reserve(pts.size());
        cleaned.push_back(pts[0]);

        for (size_t i = 1; i < pts.size(); ++i) {
            const IntPoint& prev = cleaned.back();
            const IntPoint& curr = pts[i];

            // Keep point if it's different from previous
            if (prev.x() != curr.x() || prev.y() != curr.y()) {
                cleaned.push_back(curr);
            }
        }

        // Check if last point equals first (close polygon properly)
        if (cleaned.size() > 1) {
            const IntPoint& first = cleaned.front();
            const IntPoint& last = cleaned.back();
            if (first.x() == last.x() && first.y() == last.y()) {
                cleaned.pop_back();
            }
        }

        pts = std::move(cleaned);
    };

    // Apply minimal cleaning
    removeConsecutiveDuplicates(points);

    // Warn but continue if < 3 points (let Boost handle it)
    if (points.size() < 3) {
        std::cerr << "WARNING: Polygon has < 3 points after removing duplicates ("
                  << points.size() << " points). Returning empty polygon.\n";
        return IntPolygonWithHoles();
    }

    // CRITICAL VALIDATION: Check for degenerate geometries that would crash Boost.Polygon
    // These checks are done AFTER integer conversion because the degeneracy occurs
    // in integer space due to truncation and scaling
    auto isGeometryDegenerate = [](const std::vector<IntPoint>& pts) -> bool {
        if (pts.size() < 3) return true;

        // 1. Calculate polygon area in integer space using shoelace formula
        //    Zero or near-zero area indicates degenerate geometry
        long long area2 = 0;  // 2 * area to avoid division
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t next = (i + 1) % pts.size();
            area2 += static_cast<long long>(pts[i].x()) * pts[next].y();
            area2 -= static_cast<long long>(pts[next].x()) * pts[i].y();
        }
        area2 = std::abs(area2);

        // If area is zero or extremely small, polygon is degenerate
        if (area2 < 100) {  // Threshold: less than 50 square units in integer space
            std::cerr << "WARNING: Polygon has near-zero area (" << (area2/2.0)
                      << ") in integer space - likely degenerate\n";
            return true;
        }

        // 2. Check for extremely small bounding box (collapsed polygon)
        int minX = pts[0].x(), maxX = pts[0].x();
        int minY = pts[0].y(), maxY = pts[0].y();
        for (const auto& pt : pts) {
            minX = std::min(minX, pt.x());
            maxX = std::max(maxX, pt.x());
            minY = std::min(minY, pt.y());
            maxY = std::max(maxY, pt.y());
        }
        long long bboxWidth = maxX - minX;
        long long bboxHeight = maxY - minY;

        // If bounding box is too thin (almost a line), it's degenerate
        if (bboxWidth < 2 || bboxHeight < 2) {
            std::cerr << "WARNING: Polygon has thin bounding box ("
                      << bboxWidth << " x " << bboxHeight << ") - likely degenerate\n";
            return true;
        }

        // 3. Check for excessive collinearity (many points on same line)
        //    This can confuse Boost's scanline algorithm
        int collinearCount = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t prev = (i == 0) ? pts.size() - 1 : i - 1;
            size_t next = (i + 1) % pts.size();

            // Cross product to check collinearity
            long long dx1 = pts[i].x() - pts[prev].x();
            long long dy1 = pts[i].y() - pts[prev].y();
            long long dx2 = pts[next].x() - pts[i].x();
            long long dy2 = pts[next].y() - pts[i].y();
            long long cross = dx1 * dy2 - dy1 * dx2;

            if (std::abs(cross) < 10) {  // Nearly collinear
                collinearCount++;
            }
        }

        // If more than 80% of points are collinear, likely degenerate
        if (collinearCount > pts.size() * 0.8) {
            std::cerr << "WARNING: Polygon has excessive collinearity ("
                      << collinearCount << "/" << pts.size() << " points) - likely degenerate\n";
            return true;
        }

        return false;
    };

    // Check if geometry is degenerate and reject if so
    if (isGeometryDegenerate(points)) {
        std::cerr << "ERROR: Degenerate geometry detected after integer conversion (id="
                  << poly.id << "). Rejecting to prevent Boost.Polygon crash.\n";
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

    // Convert holes with same minimal cleaning and validation
    if (!poly.children.empty()) {
        std::vector<IntPolygon> holes;
        holes.reserve(poly.children.size());

        for (const auto& hole : poly.children) {
            std::vector<IntPoint> holePoints;
            holePoints.reserve(hole.points.size());

            for (const auto& p : hole.points) {
                // CRITICAL: Use TRUNCATION for holes too
                int x = static_cast<int>(scale * p.x);
                int y = static_cast<int>(scale * p.y);
                holePoints.push_back(IntPoint(x, y));
            }

            // Minimal cleaning for holes
            removeConsecutiveDuplicates(holePoints);

            if (holePoints.size() >= 3) {
                // CRITICAL: Validate hole geometry too
                if (isGeometryDegenerate(holePoints)) {
                    std::cerr << "WARNING: Degenerate hole geometry detected (id="
                              << poly.id << "), skipping hole to prevent crash\n";
                    continue;
                }

                try {
                    IntPolygon holePoly;
                    set_points(holePoly, holePoints.begin(), holePoints.end());
                    holes.push_back(holePoly);
                }
                catch (...) {
                    std::cerr << "WARNING: Failed to create hole polygon, skipping\n";
                    continue;
                }
            } else {
                std::cerr << "WARNING: Hole has < 3 points after cleaning, skipping\n";
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

    // CRITICAL: Save reference point from B[0] for later shift
    // This matches minkowski.cc lines 286-298 (xshift, yshift)
    // NFP coordinates must be relative to B[0], not absolute
    double xshift = 0.0;
    double yshift = 0.0;
    if (!B.points.empty()) {
        xshift = B.points[0].x;
        yshift = B.points[0].y;
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
            std::cerr << "ERROR: Boost.Polygon Minkowski convolution failed: " << e.what() << std::endl;
            std::cerr << "  Polygon A(id=" << A.id << "): " << A.points.size() << " points" << std::endl;
            std::cerr << "  Polygon B(id=" << B.id << "): " << B.points.size() << " points" << std::endl;
            std::cerr << "  Scale factor: " << scale << std::endl;

            // Print first few points for debugging
            std::cerr << "  A points[0-2]: ";
            for (size_t i = 0; i < std::min(size_t(3), A.points.size()); ++i) {
                std::cerr << "(" << A.points[i].x << "," << A.points[i].y << ") ";
            }
            std::cerr << std::endl;

            std::cerr << "  B points[0-2]: ";
            for (size_t i = 0; i < std::min(size_t(3), B.points.size()); ++i) {
                std::cerr << "(" << B.points[i].x << "," << B.points[i].y << ") ";
            }
            std::cerr << std::endl;

            return std::vector<Polygon>();
        }
        catch (...) {
            std::cerr << "ERROR: Boost.Polygon Minkowski convolution failed with unknown error" << std::endl;
            std::cerr << "  Polygon A(id=" << A.id << "): " << A.points.size() << " points" << std::endl;
            std::cerr << "  Polygon B(id=" << B.id << "): " << B.points.size() << " points" << std::endl;
            std::cerr << "  Scale factor: " << scale << std::endl;

            // Print first few points for debugging
            std::cerr << "  A points[0-2]: ";
            for (size_t i = 0; i < std::min(size_t(3), A.points.size()); ++i) {
                std::cerr << "(" << A.points[i].x << "," << A.points[i].y << ") ";
            }
            std::cerr << std::endl;

            std::cerr << "  B points[0-2]: ";
            for (size_t i = 0; i < std::min(size_t(3), B.points.size()); ++i) {
                std::cerr << "(" << B.points[i].x << "," << B.points[i].y << ") ";
            }
            std::cerr << std::endl;

            return std::vector<Polygon>();
        }
    }
    // All Boost objects destroyed here - nfps contains completely independent data

    // CRITICAL: Apply reference point shift to all NFP results
    // This matches minkowski.cc lines 319-320, 480-481
    // NFP coordinates are relative to B[0], not absolute origin
    // The shift is applied because Minkowski difference is computed as A ⊖ B = A ⊕ (-B),
    // where B is negated. The resulting NFP is in a coordinate system where B's origin
    // is at (0,0). To place it correctly in the original coordinate system, we shift by B[0].
    for (auto& nfp : nfps) {
        // Shift outer boundary
        for (auto& pt : nfp.points) {
            pt.x += xshift;
            pt.y += yshift;
        }

        // Shift holes (children) if present
        for (auto& child : nfp.children) {
            for (auto& pt : child.points) {
                pt.x += xshift;
                pt.y += yshift;
            }
        }
    }

    // CRITICAL: If multiple NFPs are returned, choose the one with largest area
    // This matches background.js lines 666-673 (largest area selection)
    // Minkowski sum can produce multiple disjoint polygons, but for NFP we want
    // the largest one which represents the main no-fit region
    if (nfps.size() > 1) {
        size_t maxIndex = 0;
        double maxArea = 0.0;

        for (size_t i = 0; i < nfps.size(); ++i) {
            // Calculate absolute area using shoelace formula
            double area = 0.0;
            const auto& pts = nfps[i].points;
            for (size_t j = 0; j < pts.size(); ++j) {
                size_t next = (j + 1) % pts.size();
                area += (pts[j].x + pts[next].x) * (pts[j].y - pts[next].y);
            }
            area = std::abs(area * 0.5);

            if (area > maxArea) {
                maxArea = area;
                maxIndex = i;
            }
        }

        // Return only the largest NFP
        std::cerr << "INFO: Multiple NFPs generated (" << nfps.size()
                  << "), selecting largest with area " << maxArea << "\n";

        Polygon largestNfp = nfps[maxIndex];
        return {largestNfp};
    }

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
