#include "../../include/deepnest/converters/QtBoostConverter.h"
#include <boost/polygon/polygon.hpp>
#include <QPolygonF>
#include <cmath>

namespace deepnest {
namespace QtBoostConverter {

// ========== Point Conversions ==========

Point fromQPointF(const QPointF& point, bool exact) {
    return Point::fromQt(point, exact);
}

QPointF toQPointF(const Point& point) {
    return point.toQt();
}

// Deprecated version (no scaling)
BoostPoint qPointFToBoost(const QPointF& point) {
    // WARNING: This deprecated function doesn't apply scaling!
    // It directly casts double to int64_t, which is incorrect.
    // Use qPointFToBoost(point, scale) instead.
    return BoostPoint(
        static_cast<CoordType>(std::round(point.x())),
        static_cast<CoordType>(std::round(point.y()))
    );
}

// New scaled version
BoostPoint qPointFToBoost(const QPointF& point, double scale) {
    // Convert physical coordinates (double) to scaled integer coordinates
    // Formula: integer_coord = round(physical_coord * scale)
    return BoostPoint(
        static_cast<CoordType>(std::round(point.x() * scale)),
        static_cast<CoordType>(std::round(point.y() * scale))
    );
}

// Deprecated version (no descaling)
QPointF boostToQPointF(const BoostPoint& point) {
    // WARNING: This deprecated function doesn't apply descaling!
    // It directly casts int64_t to double, which is incorrect.
    // Use boostToQPointF(point, scale) instead.
    return QPointF(
        static_cast<double>(boost::polygon::x(point)),
        static_cast<double>(boost::polygon::y(point))
    );
}

// New descaled version
QPointF boostToQPointF(const BoostPoint& point, double scale) {
    // Convert scaled integer coordinates to physical coordinates (double)
    // Formula: physical_coord = integer_coord / scale
    return QPointF(
        static_cast<double>(boost::polygon::x(point)) / scale,
        static_cast<double>(boost::polygon::y(point)) / scale
    );
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

// Deprecated version (no scaling)
BoostPolygon toBoostPolygon(const QPainterPath& path) {
    // WARNING: This deprecated function doesn't apply scaling!
    // Use toBoostPolygon(path, scale) instead.

    if (path.isEmpty()) {
        return BoostPolygon();
    }

    QList<QPolygonF> subpaths = path.toSubpathPolygons();

    if (subpaths.isEmpty()) {
        return BoostPolygon();
    }

    const QPolygonF& firstPath = subpaths.first();
    std::vector<BoostPoint> boostPoints;
    boostPoints.reserve(firstPath.size());

    for (const auto& qpt : firstPath) {
        boostPoints.push_back(qPointFToBoost(qpt));  // Uses deprecated version
    }

    BoostPolygon result;
    boost::polygon::set_points(result, boostPoints.begin(), boostPoints.end());

    return result;
}

// New scaled version
BoostPolygon toBoostPolygon(const QPainterPath& path, double scale) {
    // Convert QPainterPath to BoostPolygon with scaling
    // This is a direct conversion for performance (no holes)

    if (path.isEmpty()) {
        return BoostPolygon();
    }

    QList<QPolygonF> subpaths = path.toSubpathPolygons();

    if (subpaths.isEmpty()) {
        return BoostPolygon();
    }

    const QPolygonF& firstPath = subpaths.first();
    std::vector<BoostPoint> boostPoints;
    boostPoints.reserve(firstPath.size());

    for (const auto& qpt : firstPath) {
        boostPoints.push_back(qPointFToBoost(qpt, scale));  // Uses scaled version
    }

    BoostPolygon result;
    boost::polygon::set_points(result, boostPoints.begin(), boostPoints.end());

    return result;
}

// Deprecated version (no scaling)
BoostPolygonWithHoles toBoostPolygonWithHoles(const QPainterPath& path) {
    // WARNING: This deprecated function doesn't apply scaling!
    // Use toBoostPolygonWithHoles(path, scale) instead.
    // Note: This will call Polygon::fromQPainterPath which also doesn't scale properly yet
    Polygon poly = Polygon::fromQPainterPath(path);
    return poly.toBoostPolygon();
}

// New scaled version
BoostPolygonWithHoles toBoostPolygonWithHoles(const QPainterPath& path, double scale) {
    // Convert via Polygon intermediate to properly handle holes
    // TODO (Step 4.3): Update Polygon::fromQPainterPath to accept scale parameter
    // For now, we'll do manual conversion

    if (path.isEmpty()) {
        return BoostPolygonWithHoles();
    }

    QList<QPolygonF> subpaths = path.toSubpathPolygons();

    if (subpaths.isEmpty()) {
        return BoostPolygonWithHoles();
    }

    // Convert first subpath as outer boundary
    const QPolygonF& outerPath = subpaths.first();
    std::vector<BoostPoint> outerPoints;
    outerPoints.reserve(outerPath.size());

    for (const auto& qpt : outerPath) {
        outerPoints.push_back(qPointFToBoost(qpt, scale));
    }

    BoostPolygonWithHoles result;
    boost::polygon::set_points(result, outerPoints.begin(), outerPoints.end());

    // Convert remaining subpaths as holes
    for (int i = 1; i < subpaths.size(); ++i) {
        const QPolygonF& holePath = subpaths[i];
        std::vector<BoostPoint> holePoints;
        holePoints.reserve(holePath.size());

        for (const auto& qpt : holePath) {
            holePoints.push_back(qPointFToBoost(qpt, scale));
        }

        BoostPolygon hole;
        boost::polygon::set_points(hole, holePoints.begin(), holePoints.end());
        boost::polygon::set_holes(result, &hole, &hole + 1);
    }

    return result;
}

// Deprecated version (no descaling)
QPainterPath fromBoostPolygon(const BoostPolygon& poly) {
    // WARNING: This deprecated function doesn't apply descaling!
    // Use fromBoostPolygon(poly, scale) instead.
    QPainterPath path;

    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin == pointsEnd) {
        return path;
    }

    auto it = pointsBegin;
    path.moveTo(boostToQPointF(*it));  // Uses deprecated version
    ++it;

    for (; it != pointsEnd; ++it) {
        path.lineTo(boostToQPointF(*it));  // Uses deprecated version
    }

    path.closeSubpath();

    return path;
}

// New descaled version
QPainterPath fromBoostPolygon(const BoostPolygon& poly, double scale) {
    // Convert BoostPolygon to QPainterPath with descaling
    QPainterPath path;

    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin == pointsEnd) {
        return path;
    }

    auto it = pointsBegin;
    path.moveTo(boostToQPointF(*it, scale));  // Uses scaled version
    ++it;

    for (; it != pointsEnd; ++it) {
        path.lineTo(boostToQPointF(*it, scale));  // Uses scaled version
    }

    path.closeSubpath();

    return path;
}

// Deprecated version (no descaling)
QPainterPath fromBoostPolygonWithHoles(const BoostPolygonWithHoles& poly) {
    // WARNING: This deprecated function doesn't apply descaling!
    // Use fromBoostPolygonWithHoles(poly, scale) instead.
    QPainterPath path;

    // Add outer boundary
    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin != pointsEnd) {
        auto it = pointsBegin;
        path.moveTo(boostToQPointF(*it));  // Uses deprecated version
        ++it;

        for (; it != pointsEnd; ++it) {
            path.lineTo(boostToQPointF(*it));  // Uses deprecated version
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
            path.moveTo(boostToQPointF(*it));  // Uses deprecated version
            ++it;

            for (; it != holePointsEnd; ++it) {
                path.lineTo(boostToQPointF(*it));  // Uses deprecated version
            }

            path.closeSubpath();
        }
    }

    return path;
}

// New descaled version
QPainterPath fromBoostPolygonWithHoles(const BoostPolygonWithHoles& poly, double scale) {
    // Convert BoostPolygonWithHoles to QPainterPath with descaling
    QPainterPath path;

    // Add outer boundary
    auto pointsBegin = boost::polygon::begin_points(poly);
    auto pointsEnd = boost::polygon::end_points(poly);

    if (pointsBegin != pointsEnd) {
        auto it = pointsBegin;
        path.moveTo(boostToQPointF(*it, scale));  // Uses scaled version
        ++it;

        for (; it != pointsEnd; ++it) {
            path.lineTo(boostToQPointF(*it, scale));  // Uses scaled version
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
            path.moveTo(boostToQPointF(*it, scale));  // Uses scaled version
            ++it;

            for (; it != holePointsEnd; ++it) {
                path.lineTo(boostToQPointF(*it, scale));  // Uses scaled version
            }

            path.closeSubpath();
        }
    }

    return path;
}

// Deprecated version (no descaling)
QPainterPath fromBoostPolygonSet(const BoostPolygonSet& polySet) {
    // WARNING: This deprecated function doesn't apply descaling!
    // Use fromBoostPolygonSet(polySet, scale) instead.
    QPainterPath path;

    std::vector<BoostPolygonWithHoles> polygons;
    polySet.get(polygons);

    for (const auto& poly : polygons) {
        path.addPath(fromBoostPolygonWithHoles(poly));  // Uses deprecated version
    }

    return path;
}

// New descaled version
QPainterPath fromBoostPolygonSet(const BoostPolygonSet& polySet, double scale) {
    // Convert BoostPolygonSet to QPainterPath with descaling
    QPainterPath path;

    std::vector<BoostPolygonWithHoles> polygons;
    polySet.get(polygons);

    for (const auto& poly : polygons) {
        path.addPath(fromBoostPolygonWithHoles(poly, scale));  // Uses scaled version
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
