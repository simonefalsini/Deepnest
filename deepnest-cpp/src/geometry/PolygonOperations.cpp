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

static Path64 toClipperPath64(const std::vector<Point>& poly, double scale) {
    Path64 path;
    path.reserve(poly.size());
    for (const auto& p : poly) {
        path.push_back(Point64(
            static_cast<int64_t>(p.x * scale),
            static_cast<int64_t>(p.y * scale)
        ));
    }
    return path;
}

static std::vector<Point> fromClipperPath64(const Path64& path, double scale) {
    std::vector<Point> poly;
    poly.reserve(path.size());
    double invScale = 1.0 / scale;
    for (const auto& p : path) {
        poly.emplace_back(
            static_cast<double>(p.x) * invScale,
            static_cast<double>(p.y) * invScale
        );
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

    // Get clipper scale from config
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    // Convert to Clipper path
    Path64 path = toClipperPath64(poly, scale);

    // Simplify to remove self-intersections
    Paths64 solution = SimplifyPaths<int64_t>({path}, scale * 0.0001);  // Small epsilon

    if (solution.empty()) {
        return {};
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

    // Convert back
    return fromClipperPath64(biggest, scale);
}

std::vector<Point> PolygonOperations::simplifyPolygon(
    const std::vector<Point>& poly,
    double distance) {

    if (poly.size() < 3) {
        return poly;
    }

    // Get clipper scale from config
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

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

    // Get clipper scale from config
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    // Convert all polygons to Clipper paths
    Paths64 paths;
    for (const auto& poly : polygons) {
        if (poly.size() >= 3) {
            paths.push_back(toClipperPath64(poly, scale));
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
        result.push_back(fromClipperPath64(path, scale));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::intersectPolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3 || polyB.size() < 3) {
        return {};
    }

    // Get clipper scale from config
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    // Convert to Clipper paths
    Path64 pathA = toClipperPath64(polyA, scale);
    Path64 pathB = toClipperPath64(polyB, scale);

    // Perform intersection
    Paths64 solution = Intersect({pathA}, {pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path, scale));
    }

    return result;
}

std::vector<std::vector<Point>> PolygonOperations::differencePolygons(
    const std::vector<Point>& polyA,
    const std::vector<Point>& polyB) {

    if (polyA.size() < 3) {
        return {};
    }

    if (polyB.size() < 3) {
        return {polyA};  // Nothing to subtract
    }

    // Get clipper scale from config
    auto& config = DeepNestConfig::getInstance();
    double scale = config.getClipperScale();

    // Convert to Clipper paths
    Path64 pathA = toClipperPath64(polyA, scale);
    Path64 pathB = toClipperPath64(polyB, scale);

    // Perform difference
    Paths64 solution = Difference({pathA}, {pathB}, FillRule::NonZero);

    // Convert results back
    std::vector<std::vector<Point>> result;
    result.reserve(solution.size());
    for (const auto& path : solution) {
        result.push_back(fromClipperPath64(path, scale));
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
