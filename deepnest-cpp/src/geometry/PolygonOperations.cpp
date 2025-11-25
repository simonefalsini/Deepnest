#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/config/DeepNestConfig.h"
#include <clipper2/clipper.h>
#include <algorithm>

namespace deepnest {

using namespace Clipper2Lib;

// Helper functions to convert between our Point type and Clipper's Point64
// Our Point already uses int64_t coordinates, so this is a direct mapping!
static Path64 toClipperPath64(const std::vector<Point>& poly) {
    Path64 path;
    path.reserve(poly.size());
    for (const auto& p : poly) {
        path.push_back(Point64(p.x, p.y));
    }
    return path;
}

static std::vector<Point> fromClipperPath64(const Path64& path) {
    std::vector<Point> poly;
    poly.reserve(path.size());
    for (const auto& p : path) {
        poly.emplace_back(static_cast<CoordType>(p.x), static_cast<CoordType>(p.y));
    }
    return poly;
}


// ========== Public Methods ==========

std::vector<std::vector<Point>> PolygonOperations::offset(
    const std::vector<Point>& poly,
    double delta,
    double miterLimit,
    double arcTolerance) {

    if (poly.size() < 3) {
        return {};
    }

    // Convert to Clipper path (int64_t native)
    Path64 path64 = toClipperPath64(poly);

    // Perform offset operation with integer coordinates
    // delta, miterLimit, arcTolerance are scaled to match our coordinate system
    Paths64 solution = InflatePaths(
        {path64},
        delta,
        JoinType::Miter,
        EndType::Polygon,
        miterLimit,
        arcTolerance
    );

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path));
    }

    return result;
}

std::vector<Point> PolygonOperations::cleanPolygon(const std::vector<Point>& poly) {
    if (poly.size() < 3) {
        return {};
    }

    // Convert to Clipper Path64 (int64_t native)
    Path64 path = toClipperPath64(poly);

    // Check for degenerate polygon before processing
    if (path.size() < 3) {
        std::cerr << "WARNING: cleanPolygon received degenerate polygon (< 3 points)" << std::endl;
        return {};
    }

    // Use integer epsilon value
    // CRITICAL FIX: epsilon must be very small for SimplifyPaths
    // SimplifyPaths removes points within epsilon distance, which is too aggressive
    // Use epsilon=0.1 (effectively minimal simplification while still removing duplicates)
    double epsilon = 0.1;

    // Simplify to remove self-intersections, collinear points, and tiny edges
    Paths64 solution = SimplifyPaths(Paths64{path}, epsilon);

    if (solution.empty()) {
        std::cerr << "WARNING: cleanPolygon SimplifyPaths returned empty result" << std::endl;
        std::cerr << "  Input polygon had " << poly.size() << " points" << std::endl;
        std::cerr << "  Epsilon: " << epsilon << std::endl;

        // Last resort: Try with larger epsilon
        solution = SimplifyPaths(Paths64{path}, 1000.0);

        if (solution.empty()) {
            std::cerr << "  Even aggressive simplification failed, returning empty" << std::endl;
            return {};
        }
        std::cerr << "  Aggressive simplification succeeded with " << solution.size() << " result(s)" << std::endl;
    }

    // Find the largest polygon (by area)
    Path64 biggest = solution[0];
    double biggestArea = std::abs(Area(biggest));

    for (size_t i = 1; i < solution.size(); i++) {
        double area = std::abs(Area(solution[i]));
        if (area > biggestArea) {
            biggest = solution[i];
            biggestArea = area;
        }
    }

    // Final validation before returning
    if (biggest.size() < 3) {
        std::cerr << "WARNING: cleanPolygon largest result has < 3 points" << std::endl;
        return {};
    }

    // Convert back
    std::vector<Point> result = fromClipperPath64(biggest);

    // Validate result
    if (result.size() < 3) {
        std::cerr << "WARNING: cleanPolygon result invalid after conversion" << std::endl;
        return {};
    }

    return result;
}

std::vector<Point> PolygonOperations::simplifyPolygon(
    const std::vector<Point>& poly,
    double distance) {

    if (poly.size() < 3) {
        return poly;
    }

    // Convert to Clipper path (int64_t native)
    Path64 path64 = toClipperPath64(poly);

    // Simplify - RamerDouglasPeucker with integer distance
    Path64 simplified = RamerDouglasPeucker(path64, distance);

    // Convert back
    return fromClipperPath64(simplified);
}

std::vector<std::vector<Point>> PolygonOperations::unionPolygons(
    const std::vector<std::vector<Point>>& polygons) {

    if (polygons.empty()) {
        return {};
    }

    // Convert all polygons to Clipper Paths64 (int64_t native)
    Paths64 paths;
    for (const auto& poly : polygons) {
        if (poly.size() >= 3) {
            paths.push_back(toClipperPath64(poly));
        }
    }

    if (paths.empty()) {
        return {};
    }

    // Perform union
    Paths64 solution = Union(paths, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::intersectPolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3 || polyB.size() < 3) {
        return {};
    }

    // Convert to Clipper Paths64 (int64_t native)
    Path64 pathA = toClipperPath64(polyA);
    Path64 pathB = toClipperPath64(polyB);

    // Perform intersection
    Paths64 solution = Intersect(Paths64{pathA}, Paths64{pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::differencePolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3 || polyB.size() < 3) {
        return {};
    }

    // Convert to Clipper Paths64 (int64_t native)
    Path64 pathA = toClipperPath64(polyA);
    Path64 pathB = toClipperPath64(polyB);

    // Perform difference
    Paths64 solution = Difference(Paths64{pathA}, Paths64{pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path));
    }

    return result;
}

// DEPRECATED: These functions are no longer needed since Point already uses int64_t
// Our coordinates are already in the correct integer format for Clipper2
std::vector<std::pair<long long, long long>> PolygonOperations::toClipperCoordinates(
    const std::vector<Point>& poly,
    double scale) {

    // WARNING: This function is deprecated and shouldn't be used
    // Point coordinates are already int64_t, no additional scaling needed
    std::vector<std::pair<long long, long long>> result;
    result.reserve(poly.size());

    for (const auto& p : poly) {
        result.emplace_back(
            static_cast<long long>(p.x * scale),
            static_cast<long long>(p.y * scale)
        );
    }

    return result;
}

// DEPRECATED: These functions are no longer needed since Point already uses int64_t
std::vector<Point> PolygonOperations::fromClipperCoordinates(
    const std::vector<std::pair<long long, long long>>& path,
    double scale) {

    // WARNING: This function is deprecated and shouldn't be used
    // Point coordinates are already int64_t, no descaling needed
    std::vector<Point> result;
    result.reserve(path.size());

    double invScale = 1.0 / scale;
    for (const auto& p : path) {
        result.emplace_back(
            static_cast<CoordType>(static_cast<double>(p.first) * invScale),
            static_cast<CoordType>(static_cast<double>(p.second) * invScale)
        );
    }

    return result;
}

double PolygonOperations::area(const std::vector<Point>& poly) {
    if (poly.size() < 3) {
        return 0.0;
    }

    // Use Clipper's area function with Path64 (int64_t native)
    Path64 path64 = toClipperPath64(poly);
    return Area(path64);
}

std::vector<Point> PolygonOperations::reversePolygon(const std::vector<Point>& poly) {
    std::vector<Point> reversed(poly.rbegin(), poly.rend());
    return reversed;
}

} // namespace deepnest
