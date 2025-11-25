#ifndef DEEPNEST_POLYGONOPERATIONS_H
#define DEEPNEST_POLYGONOPERATIONS_H

#include "../core/Types.h"
#include "../core/Point.h"
#include <vector>

namespace deepnest {

// Forward declaration
class Polygon;

/**
 * @brief Polygon operations using Clipper2 library
 *
 * This class provides polygon operations such as offset, simplification,
 * and boolean operations using the Clipper2 library.
 * Based on polygonOffset and cleanPolygon functions from deepnest.js
 */
class PolygonOperations {
public:
    /**
     * @brief Perform offset operation on a polygon
     *
     * Positive offset expands the polygon, negative contracts it.
     * This function uses ClipperOffset with miter joins.
     *
     * @param poly Input polygon
     * @param delta Offset distance (in same units as polygon)
     * @param miterLimit Miter limit for offset (default 4.0)
     * @param arcTolerance Arc tolerance for curved offsets (default from config)
     * @return Vector of polygons resulting from offset operation
     */
    static std::vector<std::vector<Point>> offset(
        const std::vector<Point>& poly,
        double delta,
        double miterLimit = 4.0,
        double arcTolerance = 0.3
    );

    /**
     * @brief Clean a polygon by removing self-intersections
     *
     * Uses Clipper's SimplifyPolygon function to remove self-intersections
     * and returns the largest resulting polygon.
     *
     * @param poly Input polygon
     * @param fillType Clipper fill type (default NonZero)
     * @return Cleaned polygon, or empty vector if cleaning failed
     */
    static std::vector<Point> cleanPolygon(const std::vector<Point>& poly);

    /**
     * @brief Simplify a polygon by reducing number of vertices
     *
     * Uses Clipper's CleanPolygon to remove vertices that are too close together
     *
     * @param poly Input polygon
     * @param distance Minimum distance between vertices
     * @return Simplified polygon
     */
    static std::vector<Point> simplifyPolygon(
        const std::vector<Point>& poly,
        double distance = 0.1
    );

    /**
     * @brief Perform union operation on multiple polygons
     *
     * @param polygons Vector of polygons to union
     * @return Vector of resulting polygons from union
     */
    static std::vector<std::vector<Point>> unionPolygons(
        const std::vector<std::vector<Point>>& polygons
    );

    /**
     * @brief Perform intersection operation on two polygons
     *
     * @param polyA First polygon
     * @param polyB Second polygon
     * @return Vector of resulting polygons from intersection
     */
    static std::vector<std::vector<Point>> intersectPolygons(
        const std::vector<Point>& polyA,
        const std::vector<Point>& polyB
    );

    /**
     * @brief Perform difference operation on two polygons
     *
     * @param polyA First polygon (subject)
     * @param polyB Second polygon (clip)
     * @return Vector of resulting polygons (polyA - polyB)
     */
    static std::vector<std::vector<Point>> differencePolygons(
        const std::vector<Point>& polyA,
        const std::vector<Point>& polyB
    );

    /**
     * @brief Convert polygon points to Clipper coordinates
     *
     * @deprecated This function is no longer needed since Point already uses int64_t coordinates.
     * Our Point coordinates are already in the correct integer format for Clipper2.
     * Use toClipperPath64() helper instead.
     *
     * @param poly Input polygon
     * @param scale Scaling factor (deprecated, not needed)
     * @return Clipper path with scaled integer coordinates
     */
    static std::vector<std::pair<long long, long long>> toClipperCoordinates(
        const std::vector<Point>& poly,
        double scale = 10000000.0
    );

    /**
     * @brief Convert Clipper coordinates back to polygon points
     *
     * @deprecated This function is no longer needed since Point already uses int64_t coordinates.
     * Use fromClipperPath64() helper instead.
     *
     * @param path Clipper path with integer coordinates
     * @param scale Scaling factor (deprecated, not needed)
     * @return Polygon
     */
    static std::vector<Point> fromClipperCoordinates(
        const std::vector<std::pair<long long, long long>>& path,
        double scale = 10000000.0
    );

    /**
     * @brief Calculate area of polygon using Clipper
     *
     * @param poly Input polygon
     * @return Signed area (positive for CCW, negative for CW)
     */
    static double area(const std::vector<Point>& poly);

    /**
     * @brief Reverse winding order of polygon
     *
     * @param poly Input polygon
     * @return Polygon with reversed winding order
     */
    static std::vector<Point> reversePolygon(const std::vector<Point>& poly);

private:
    // Default clipper scale from config
    static constexpr double DEFAULT_CLIPPER_SCALE = 10000000.0;
};

} // namespace deepnest

#endif // DEEPNEST_POLYGONOPERATIONS_H
