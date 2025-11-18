#include "../../include/deepnest/converters/QtBoostConverter.h"
#include <boost/polygon/polygon.hpp>
#include <QPolygonF>

namespace deepnest {
namespace QtBoostConverter {

// ========== Point Conversions ==========

Point fromQPointF(const QPointF& point, bool exact) {
    return Point::fromQt(point, exact);
}

QPointF toQPointF(const Point& point) {
    return point.toQt();
}

BoostPoint qPointFToBoost(const QPointF& point) {
    return BoostPoint(point.x(), point.y());
}

QPointF boostToQPointF(const BoostPoint& point) {
    return QPointF(boost::polygon::x(point), boost::polygon::y(point));
}

std::vector<Point> fromQPointFList(const QList<QPointF>& points, bool exact) {
    std::vector<Point> result;
    result.reserve(points.size());

    for (const auto& qpt : points) {
        result.push_back(Point::fromQt(qpt, exact));
    }

    return result;
}

QList<QPointF> toQPointFList(const std::vector<Point>& points) {
    QList<QPointF> result;
    result.reserve(points.size());

    for (const auto& pt : points) {
        result.append(pt.toQt());
    }

    return result;
}

// ========== Polygon Conversions ==========

Polygon fromQPainterPath(const QPainterPath& path, int polygonId) {
    return Polygon::fromQPainterPath(path, polygonId);
}

QPainterPath toQPainterPath(const Polygon& polygon) {
    return polygon.toQPainterPath();
}

std::vector<Polygon> extractPolygonsFromQPainterPath(const QPainterPath& path) {
    return Polygon::extractFromQPainterPath(path);
}

QPainterPath toQPainterPath(const std::vector<Polygon>& polygons) {
    QPainterPath result;

    for (const auto& polygon : polygons) {
        result.addPath(polygon.toQPainterPath());
    }

    return result;
}

// ========== Direct Boost-Qt Conversions ==========

BoostPolygon toBoostPolygon(const QPainterPath& path) {
    // Convert QPainterPath to simple BoostPolygon (no holes)
    // This is a direct conversion for performance

    if (path.isEmpty()) {
        return BoostPolygon();
    }

    // Extract points from the first subpath
    QList<QPolygonF> subpaths = path.toSubpathPolygons();

    if (subpaths.isEmpty()) {
        return BoostPolygon();
    }

    const QPolygonF& firstPath = subpaths.first();
    std::vector<BoostPoint> boostPoints;
    boostPoints.reserve(firstPath.size());

    for (const auto& qpt : firstPath) {
        boostPoints.push_back(qPointFToBoost(qpt));
    }

    BoostPolygon result;
    boost::polygon::set_points(result, boostPoints.begin(), boostPoints.end());

    return result;
}

BoostPolygonWithHoles toBoostPolygonWithHoles(const QPainterPath& path) {
    // Convert via Polygon intermediate to properly handle holes
    Polygon poly = Polygon::fromQPainterPath(path);
    return poly.toBoostPolygon();
}

QPainterPath fromBoostPolygon(const BoostPolygon& poly) {
    QPainterPath path;

    // Extract points from BoostPolygon
    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin == pointsEnd) {
        return path;
    }

    // Start the path
    auto it = pointsBegin;
    path.moveTo(boostToQPointF(*it));
    ++it;

    // Add remaining points
    for (; it != pointsEnd; ++it) {
        path.lineTo(boostToQPointF(*it));
    }

    path.closeSubpath();

    return path;
}

QPainterPath fromBoostPolygonWithHoles(const BoostPolygonWithHoles& poly) {
    QPainterPath path;

    // Add outer boundary
    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin != pointsEnd) {
        auto it = pointsBegin;
        path.moveTo(boostToQPointF(*it));
        ++it;

        for (; it != pointsEnd; ++it) {
            path.lineTo(boostToQPointF(*it));
        }

        path.closeSubpath();
    }

    // Add holes
    auto holesBegin = boost::polygon::begin_holes(poly);
    auto holesEnd = boost::polygon::end_holes(poly);

    for (auto holeIt = holesBegin; holeIt != holesEnd; ++holeIt) {
        auto holePointsBegin = boost::polygon::begin_points(*holeIt);
        auto holePointsEnd = boost::polygon::end_points(*holeIt);

        if (holePointsBegin != holePointsEnd) {
            auto it = holePointsBegin;
            path.moveTo(boostToQPointF(*it));
            ++it;

            for (; it != holePointsEnd; ++it) {
                path.lineTo(boostToQPointF(*it));
            }

            path.closeSubpath();
        }
    }

    return path;
}

QPainterPath fromBoostPolygonSet(const BoostPolygonSet& polySet) {
    QPainterPath path;

    // Extract all polygons from the set
    std::vector<BoostPolygonWithHoles> polygons;
    polySet.get(polygons);

    // Convert each polygon to a subpath
    for (const auto& poly : polygons) {
        path.addPath(fromBoostPolygonWithHoles(poly));
    }

    return path;
}

// ========== Utility Functions ==========

Polygon fromQPolygonF(const QPolygonF& qpoly, int polygonId) {
    Polygon result;
    result.id = polygonId;

    result.points.reserve(qpoly.size());
    for (const auto& qpt : qpoly) {
        result.points.push_back(Point::fromQt(qpt));
    }

    return result;
}

QPolygonF toQPolygonF(const Polygon& polygon) {
    QPolygonF result;
    result.reserve(polygon.points.size());

    for (const auto& pt : polygon.points) {
        result.append(pt.toQt());
    }

    return result;
}

} // namespace QtBoostConverter
} // namespace deepnest
