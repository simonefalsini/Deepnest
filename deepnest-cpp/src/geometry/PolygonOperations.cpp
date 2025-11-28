#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/config/DeepNestConfig.h"
#include <clipper2/clipper.h>
#include <algorithm>

namespace deepnest {

using namespace Clipper2Lib;

// Helper functions to convert between our Point type and Clipper's Point64
static PathD toClipperPathD(const std::vector<Point>& poly) {
    PathD path;
    path.reserve(poly.size());
    for (const auto& p : poly) {
        path.push_back(PointD(p.x, p.y));
    }
    return path;
}

static std::vector<Point> fromClipperPathD(const PathD& path) {
    std::vector<Point> poly;
    poly.reserve(path.size());
    for (const auto& p : path) {
        poly.emplace_back(p.x, p.y);
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

    // Convert to Clipper path
    PathD pathD = toClipperPathD(poly);

    // Perform offset operation
    PathsD solution = InflatePaths(
        {pathD},
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
        result.push_back(fromClipperPathD(path));
    }

    return result;
}

std::vector<Point> PolygonOperations::cleanPolygon(const std::vector<Point>& poly) {
    if (poly.size() < 3) {
        return {};
    }

    // Convert to Clipper PathD (double precision, no scaling needed)
    PathD path = toClipperPathD(poly);

    // Check for degenerate polygon before processing
    if (path.size() < 3) {
        std::cerr << "WARNING: cleanPolygon received degenerate polygon (< 3 points)" << std::endl;
        return {};
    }

    // Use absolute epsilon value (no scaling needed with PathsD)
    // 0.01 provides good balance between cleaning and preserving shape
    double epsilon = 0.01;

    // Simplify to remove self-intersections, collinear points, and tiny edges
    PathsD solution = SimplifyPaths(PathsD{path}, epsilon);

    if (solution.empty()) {
        std::cerr << "WARNING: cleanPolygon SimplifyPaths returned empty result" << std::endl;
        std::cerr << "  Input polygon had " << poly.size() << " points" << std::endl;
        std::cerr << "  Epsilon: " << epsilon << std::endl;

        // Last resort: Try with larger epsilon
        solution = SimplifyPaths(PathsD{path}, 0.1);

        if (solution.empty()) {
            std::cerr << "  Even aggressive simplification failed, returning empty" << std::endl;
            return {};
        }
        std::cerr << "  Aggressive simplification succeeded with " << solution.size() << " result(s)" << std::endl;
    }

    // Find the largest polygon (by area)
    PathD biggest = solution[0];
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

    // Convert back (no unscaling needed)
    std::vector<Point> result = fromClipperPathD(biggest);

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

    // Convert to Clipper path
    PathD pathD = toClipperPathD(poly);

    // Simplify - RamerDouglasPeucker
    PathD simplified = RamerDouglasPeucker(pathD, distance);

    // Convert back
    return fromClipperPathD(simplified);
}

std::vector<std::vector<Point>> PolygonOperations::unionPolygons(
    const std::vector<std::vector<Point>>& polygons) {

    if (polygons.empty()) {
        return {};
    }

    // Convert all polygons to Clipper PathsD (no scaling needed)
    PathsD paths;
    for (const auto& poly : polygons) {
        if (poly.size() >= 3) {
            paths.push_back(toClipperPathD(poly));
        }
    }

    if (paths.empty()) {
        return {};
    }

    // Perform union
    PathsD solution = Union(paths, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPathD(path));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::intersectPolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3 || polyB.size() < 3) {
        return {};
    }

    // Convert to Clipper PathsD (no scaling needed)
    PathD pathA = toClipperPathD(polyA);
    PathD pathB = toClipperPathD(polyB);

    // Perform intersection
    PathsD solution = Intersect(PathsD{pathA}, PathsD{pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPathD(path));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::differencePolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3 || polyB.size() < 3) {
        return {};
    }

    // Convert to Clipper PathsD (no scaling needed)
    PathD pathA = toClipperPathD(polyA);
    PathD pathB = toClipperPathD(polyB);

    // Perform difference
    PathsD solution = Difference(PathsD{pathA}, PathsD{pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPathD(path));
    }

    return result;
}

std::vector<std::pair<long long, long long>> PolygonOperations::toClipperCoordinates(
    const std::vector<Point>& poly,
    double scale) {

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

std::vector<Point> PolygonOperations::fromClipperCoordinates(
    const std::vector<std::pair<long long, long long>>& path,
    double scale) {

    std::vector<Point> result;
    result.reserve(path.size());

    double invScale = 1.0 / scale;
    for (const auto& p : path) {
        result.emplace_back(
            static_cast<double>(p.first) * invScale,
            static_cast<double>(p.second) * invScale
        );
    }

    return result;
}

double PolygonOperations::area(const std::vector<Point>& poly) {
    if (poly.size() < 3) {
        return 0.0;
    }

    // Use Clipper's area function
    PathD pathD = toClipperPathD(poly);
    return Area(pathD);
}

std::vector<Point> PolygonOperations::reversePolygon(const std::vector<Point>& poly) {
    std::vector<Point> reversed(poly.rbegin(), poly.rend());
    return reversed;
}

} // namespace deepnest
