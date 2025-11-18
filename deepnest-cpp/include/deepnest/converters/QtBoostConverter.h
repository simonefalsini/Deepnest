#ifndef DEEPNEST_QT_BOOST_CONVERTER_H
#define DEEPNEST_QT_BOOST_CONVERTER_H

#include "../core/Types.h"
#include "../core/Point.h"
#include "../core/Polygon.h"
#include <QPainterPath>
#include <QPointF>
#include <vector>

namespace deepnest {

/**
 * @brief Namespace for Qt-Boost type conversions
 *
 * This namespace provides a centralized interface for converting between
 * Qt types (QPainterPath, QPointF) and Boost.Polygon types.
 *
 * The conversions are organized into three main categories:
 * 1. Point conversions (Point <-> QPointF <-> BoostPoint)
 * 2. Polygon conversions (Polygon <-> QPainterPath <-> BoostPolygon)
 * 3. Direct conversions (QPainterPath <-> BoostPolygon without Polygon intermediate)
 */
namespace QtBoostConverter {

    // ========== Point Conversions ==========

    /**
     * @brief Convert QPointF to deepnest::Point
     *
     * @param point Qt point to convert
     * @param exact Whether this point should be marked as exact (for merge detection)
     * @return Converted Point
     */
    Point fromQPointF(const QPointF& point, bool exact = false);

    /**
     * @brief Convert deepnest::Point to QPointF
     *
     * @param point Point to convert
     * @return Converted QPointF
     */
    QPointF toQPointF(const Point& point);

    /**
     * @brief Convert QPointF to BoostPoint
     *
     * @param point Qt point to convert
     * @return Converted BoostPoint
     */
    BoostPoint qPointFToBoost(const QPointF& point);

    /**
     * @brief Convert BoostPoint to QPointF
     *
     * @param point Boost point to convert
     * @return Converted QPointF
     */
    QPointF boostToQPointF(const BoostPoint& point);

    /**
     * @brief Convert vector of QPointF to vector of Points
     *
     * @param points Qt points to convert
     * @param exact Whether points should be marked as exact
     * @return Vector of converted Points
     */
    std::vector<Point> fromQPointFList(const QList<QPointF>& points, bool exact = false);

    /**
     * @brief Convert vector of Points to vector of QPointF
     *
     * @param points Points to convert
     * @return Vector of converted QPointF
     */
    QList<QPointF> toQPointFList(const std::vector<Point>& points);

    // ========== Polygon Conversions ==========

    /**
     * @brief Convert QPainterPath to deepnest::Polygon
     *
     * This conversion handles simple paths and automatically detects holes.
     * For complex paths with multiple disconnected subpaths, use extractPolygonsFromQPainterPath.
     *
     * @param path Qt painter path to convert
     * @param polygonId Optional ID to assign to the polygon
     * @return Converted Polygon
     */
    Polygon fromQPainterPath(const QPainterPath& path, int polygonId = -1);

    /**
     * @brief Convert deepnest::Polygon to QPainterPath
     *
     * This conversion includes the outer boundary and all holes (children).
     *
     * @param polygon Polygon to convert
     * @return Converted QPainterPath
     */
    QPainterPath toQPainterPath(const Polygon& polygon);

    /**
     * @brief Extract multiple polygons from a QPainterPath
     *
     * QPainterPath can contain multiple disconnected subpaths.
     * This function extracts each subpath as a separate polygon.
     *
     * @param path Qt painter path to extract from
     * @return Vector of extracted polygons
     */
    std::vector<Polygon> extractPolygonsFromQPainterPath(const QPainterPath& path);

    /**
     * @brief Convert vector of Polygons to QPainterPath
     *
     * Combines multiple polygons into a single QPainterPath with multiple subpaths.
     *
     * @param polygons Polygons to convert
     * @return Combined QPainterPath
     */
    QPainterPath toQPainterPath(const std::vector<Polygon>& polygons);

    // ========== Direct Boost-Qt Conversions ==========

    /**
     * @brief Convert QPainterPath directly to BoostPolygon
     *
     * This is a direct conversion that bypasses the Polygon intermediate type.
     * Useful for performance-critical operations.
     *
     * Note: This handles simple polygons without holes. For polygons with holes,
     * use toBoostPolygonWithHoles instead.
     *
     * @param path Qt painter path to convert
     * @return Converted BoostPolygon (simple polygon, no holes)
     */
    BoostPolygon toBoostPolygon(const QPainterPath& path);

    /**
     * @brief Convert QPainterPath directly to BoostPolygonWithHoles
     *
     * This conversion handles polygons with holes.
     *
     * @param path Qt painter path to convert
     * @return Converted BoostPolygonWithHoles
     */
    BoostPolygonWithHoles toBoostPolygonWithHoles(const QPainterPath& path);

    /**
     * @brief Convert BoostPolygon to QPainterPath
     *
     * Converts a simple Boost polygon (no holes) to QPainterPath.
     *
     * @param poly Boost polygon to convert
     * @return Converted QPainterPath
     */
    QPainterPath fromBoostPolygon(const BoostPolygon& poly);

    /**
     * @brief Convert BoostPolygonWithHoles to QPainterPath
     *
     * Converts a Boost polygon with holes to QPainterPath.
     *
     * @param poly Boost polygon with holes to convert
     * @return Converted QPainterPath
     */
    QPainterPath fromBoostPolygonWithHoles(const BoostPolygonWithHoles& poly);

    /**
     * @brief Convert BoostPolygonSet to QPainterPath
     *
     * Converts a Boost polygon set (result of boolean operations) to QPainterPath.
     * Each polygon in the set becomes a subpath.
     *
     * @param polySet Boost polygon set to convert
     * @return Converted QPainterPath with multiple subpaths
     */
    QPainterPath fromBoostPolygonSet(const BoostPolygonSet& polySet);

    // ========== Utility Functions ==========

    /**
     * @brief Convert QPolygonF to deepnest::Polygon
     *
     * Helper function for converting Qt's QPolygonF to our Polygon type.
     *
     * @param qpoly Qt polygon to convert
     * @param polygonId Optional ID to assign
     * @return Converted Polygon
     */
    Polygon fromQPolygonF(const QPolygonF& qpoly, int polygonId = -1);

    /**
     * @brief Convert deepnest::Polygon to QPolygonF
     *
     * Helper function for converting our Polygon to Qt's QPolygonF.
     * Note: This only converts the outer boundary, holes are ignored.
     *
     * @param polygon Polygon to convert
     * @return Converted QPolygonF
     */
    QPolygonF toQPolygonF(const Polygon& polygon);

} // namespace QtBoostConverter

} // namespace deepnest

#endif // DEEPNEST_QT_BOOST_CONVERTER_H
