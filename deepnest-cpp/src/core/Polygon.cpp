#include "../../include/deepnest/core/Polygon.h"
#include "../../include/deepnest/geometry/Transformation.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include <boost/polygon/polygon.hpp>
#include <algorithm>
#include <numeric>

namespace deepnest {

// ========== Constructors ==========

Polygon::Polygon()
    : id(-1)
    , source(-1)
    , rotation(0.0)
    , quantity(1)
    , isSheet(false)
{}

Polygon::Polygon(const std::vector<Point>& pts)
    : points(pts)
    , id(-1)
    , source(-1)
    , rotation(0.0)
    , quantity(1)
    , isSheet(false)
{}

Polygon::Polygon(const std::vector<Point>& pts, int polygonId)
    : points(pts)
    , id(polygonId)
    , source(-1)
    , rotation(0.0)
    , quantity(1)
    , isSheet(false)
{}

// ========== Geometric Properties ==========

double Polygon::area() const {
    if (points.size() < 3) {
        return 0.0;
    }
    return GeometryUtil::polygonArea(points);
}

BoundingBox Polygon::bounds() const {
    if (points.empty()) {
        return BoundingBox();
    }
    return GeometryUtil::getPolygonBounds(points);
}

bool Polygon::isValid() const {
    return points.size() >= 3;
}

void Polygon::reverse() {
    std::reverse(points.begin(), points.end());

    // Also reverse all holes
    for (auto& hole : children) {
        hole.reverse();
    }
}

Polygon Polygon::reversed() const {
    Polygon result = *this;
    result.reverse();
    return result;
}

bool Polygon::isCounterClockwise() const {
    return area() > 0.0;
}

// ========== Transformations ==========

Polygon Polygon::rotate(double angleDegrees) const {
    Transformation t;
    t.rotate(angleDegrees);
    return transform(t);
}

Polygon Polygon::rotateAround(double angleDegrees, const Point& center) const {
    Transformation t;
    t.rotate(angleDegrees, center.x, center.y);
    return transform(t);
}

Polygon Polygon::translate(double dx, double dy) const {
    Transformation t;
    t.translate(dx, dy);
    return transform(t);
}

Polygon Polygon::translate(const Point& offset) const {
    return translate(offset.x, offset.y);
}

Polygon Polygon::scale(double factor) const {
    return scale(factor, factor);
}

Polygon Polygon::scale(double sx, double sy) const {
    Transformation t;
    t.scale(sx, sy);
    return transform(t);
}

Polygon Polygon::transform(const Transformation& t) const {
    Polygon result;
    result.points = t.apply(points);

    // Transform all holes
    result.children.reserve(children.size());
    for (const auto& hole : children) {
        result.children.push_back(hole.transform(t));
    }

    // Copy metadata
    result.updateMetadataAfterTransform(*this);

    return result;
}

void Polygon::updateMetadataAfterTransform(const Polygon& source) {
    this->id = source.id;
    this->source = source.source;
    this->rotation = source.rotation;
    this->quantity = source.quantity;
    this->isSheet = source.isSheet;
    this->name = source.name;
}

// ========== Conversions ==========

BoostPolygonWithHoles Polygon::toBoostPolygon() const {
    // Create outer polygon
    std::vector<BoostPoint> boostPoints;
    boostPoints.reserve(points.size());
    for (const auto& p : points) {
        boostPoints.push_back(p.toBoost());
    }

    BoostPolygonWithHoles result;
    boost::polygon::set_points(result, boostPoints.begin(), boostPoints.end());

    // Add holes
    if (!children.empty()) {
        std::vector<BoostPolygon> holes;
        holes.reserve(children.size());

        for (const auto& child : children) {
            std::vector<BoostPoint> holePoints;
            holePoints.reserve(child.points.size());
            for (const auto& p : child.points) {
                holePoints.push_back(p.toBoost());
            }

            BoostPolygon holePoly;
            boost::polygon::set_points(holePoly, holePoints.begin(), holePoints.end());
            holes.push_back(holePoly);
        }

        boost::polygon::set_holes(result, holes.begin(), holes.end());
    }

    return result;
}

Polygon Polygon::fromBoostPolygon(const BoostPolygonWithHoles& boostPoly) {
    Polygon result;

    // Extract outer boundary
    for (auto it = boost::polygon::begin_points(boostPoly);
         it != boost::polygon::end_points(boostPoly); ++it) {
        result.points.push_back(Point::fromBoost(*it));
    }

    // Extract holes
    for (auto holeIt = boost::polygon::begin_holes(boostPoly);
         holeIt != boost::polygon::end_holes(boostPoly); ++holeIt) {
        Polygon hole;
        for (auto pointIt = boost::polygon::begin_points(*holeIt);
             pointIt != boost::polygon::end_points(*holeIt); ++pointIt) {
            hole.points.push_back(Point::fromBoost(*pointIt));
        }
        result.children.push_back(hole);
    }

    return result;
}

QPainterPath Polygon::toQPainterPath() const {
    QPainterPath path;

    if (points.empty()) {
        return path;
    }

    // Add outer boundary
    path.moveTo(points[0].toQt());
    for (size_t i = 1; i < points.size(); i++) {
        path.lineTo(points[i].toQt());
    }
    path.closeSubpath();

    // Add holes
    for (const auto& hole : children) {
        if (!hole.points.empty()) {
            path.moveTo(hole.points[0].toQt());
            for (size_t i = 1; i < hole.points.size(); i++) {
                path.lineTo(hole.points[i].toQt());
            }
            path.closeSubpath();
        }
    }

    return path;
}

Polygon Polygon::fromQPainterPath(const QPainterPath& path) {
    return fromQPainterPath(path, -1);
}

Polygon Polygon::fromQPainterPath(const QPainterPath& path, int polygonId) {
    Polygon result;
    result.id = polygonId;

    if (path.isEmpty()) {
        return result;
    }

    // CRITICAL FIX: Simplify path BEFORE conversion to remove degenerate geometries
    // and redundant curve control points that cause Boost.Polygon scanline failures
    // Step 1: Set proper fill rule for complex paths
    QPainterPath cleanPath = path;
    cleanPath.setFillRule(Qt::WindingFill);

    // Step 2: Apply simplified() which merges overlapping/degenerate regions
    cleanPath = cleanPath.simplified();

    // Convert QPainterPath to polygon points
    // Use Qt's toSubpathPolygons() which automatically converts curves to line segments
    QList<QPolygonF> subpaths = cleanPath.toSubpathPolygons();

    if (subpaths.isEmpty()) {
        return result;
    }

    // Use the first subpath as the outer boundary
    const QPolygonF& firstSubpath = subpaths.first();

    // CRITICAL FIX: Remove duplicate consecutive points that cause Boost.Polygon issues
    // This can happen when curves are converted to line segments
    std::vector<Point> cleanedPoints;
    cleanedPoints.reserve(firstSubpath.size());

    constexpr double MIN_DISTANCE_SQ = 0.001;  // Minimum squared distance between points

    for (int i = 0; i < firstSubpath.size(); ++i) {
        Point pt = Point::fromQt(firstSubpath[i]);

        // Check if this point is too close to the previous point
        if (!cleanedPoints.empty()) {
            const Point& prev = cleanedPoints.back();
            double dx = pt.x - prev.x;
            double dy = pt.y - prev.y;
            double distSq = dx * dx + dy * dy;

            if (distSq < MIN_DISTANCE_SQ) {
                continue;  // Skip this point - too close to previous
            }
        }

        cleanedPoints.push_back(pt);
    }

    // Also check if first and last points are duplicates (closed path)
    if (cleanedPoints.size() >= 2) {
        const Point& first = cleanedPoints.front();
        const Point& last = cleanedPoints.back();
        double dx = first.x - last.x;
        double dy = first.y - last.y;
        double distSq = dx * dx + dy * dy;

        if (distSq < MIN_DISTANCE_SQ) {
            cleanedPoints.pop_back();  // Remove duplicate closing point
        }
    }

    // Validate minimum points for a polygon
    if (cleanedPoints.size() < 3) {
        return result;  // Invalid polygon
    }

    result.points = std::move(cleanedPoints);

    // Additional subpaths become holes/children
    // Note: This is a simplified approach - a full implementation would need to
    // determine which subpaths are holes vs separate polygons based on winding order
    for (int i = 1; i < subpaths.size(); ++i) {
        const QPolygonF& subpath = subpaths[i];

        Polygon hole;
        hole.points.reserve(subpath.size());
        for (const QPointF& pt : subpath) {
            hole.points.push_back(Point::fromQt(pt));
        }

        if (hole.isValid()) {
            result.children.push_back(hole);
        }
    }

    return result;
}

std::vector<Polygon> Polygon::extractFromQPainterPath(const QPainterPath& path) {
    std::vector<Polygon> polygons;

    // QPainterPath can contain multiple disconnected subpaths
    // We need to extract each one as a separate polygon

    // CRITICAL FIX: Apply same cleaning as fromQPainterPath for consistency
    QPainterPath cleanPath = path;
    cleanPath.setFillRule(Qt::WindingFill);
    cleanPath = cleanPath.simplified();
    QList<QPolygonF> qtPolygons = cleanPath.toSubpathPolygons();

    constexpr double MIN_DISTANCE_SQ = 0.001;

    for (const auto& qtPoly : qtPolygons) {
        if (qtPoly.size() < 3) {
            continue;
        }

        // Clean up duplicate/near-duplicate points
        std::vector<Point> cleanedPoints;
        cleanedPoints.reserve(qtPoly.size());

        for (const auto& qpt : qtPoly) {
            Point pt = Point::fromQt(qpt);

            if (!cleanedPoints.empty()) {
                const Point& prev = cleanedPoints.back();
                double dx = pt.x - prev.x;
                double dy = pt.y - prev.y;
                double distSq = dx * dx + dy * dy;

                if (distSq < MIN_DISTANCE_SQ) {
                    continue;
                }
            }

            cleanedPoints.push_back(pt);
        }

        // Check closing point
        if (cleanedPoints.size() >= 2) {
            const Point& first = cleanedPoints.front();
            const Point& last = cleanedPoints.back();
            double dx = first.x - last.x;
            double dy = first.y - last.y;
            double distSq = dx * dx + dy * dy;

            if (distSq < MIN_DISTANCE_SQ) {
                cleanedPoints.pop_back();
            }
        }

        if (cleanedPoints.size() >= 3) {
            Polygon poly;
            poly.points = std::move(cleanedPoints);

            if (poly.isValid()) {
                polygons.push_back(poly);
            }
        }
    }

    return polygons;
}

// ========== Utilities ==========

Polygon Polygon::clone() const {
    return *this;
}

Polygon Polygon::copy(int newId) const {
    Polygon result = *this;
    result.id = newId;
    return result;
}

void Polygon::addHole(const Polygon& hole) {
    children.push_back(hole);
}

void Polygon::clearHoles() {
    children.clear();
}

Point Polygon::centroid() const {
    if (points.empty()) {
        return Point();
    }

    if (points.size() == 1) {
        return points[0];
    }

    // Calculate centroid using area-weighted formula
    double cx = 0.0, cy = 0.0;
    double signedArea = 0.0;

    for (size_t i = 0; i < points.size(); i++) {
        size_t j = (i + 1) % points.size();
        double x0 = points[i].x;
        double y0 = points[i].y;
        double x1 = points[j].x;
        double y1 = points[j].y;
        double a = x0 * y1 - x1 * y0;
        signedArea += a;
        cx += (x0 + x1) * a;
        cy += (y0 + y1) * a;
    }

    signedArea *= 0.5;

    if (Point::almostEqual(signedArea, 0.0)) {
        // Degenerate polygon, just return average of points
        cx = 0.0;
        cy = 0.0;
        for (const auto& p : points) {
            cx += p.x;
            cy += p.y;
        }
        return Point(cx / points.size(), cy / points.size());
    }

    cx /= (6.0 * signedArea);
    cy /= (6.0 * signedArea);

    return Point(cx, cy);
}

std::vector<Polygon> Polygon::offset(double distance) const {
    auto offsetPolygons = PolygonOperations::offset(points, distance);

    std::vector<Polygon> result;
    result.reserve(offsetPolygons.size());

    for (const auto& poly : offsetPolygons) {
        Polygon p(poly);
        p.id = this->id;
        p.source = this->source;
        p.rotation = this->rotation;
        p.name = this->name;
        result.push_back(p);
    }

    return result;
}

Polygon Polygon::simplify(double tolerance) const {
    Polygon result;
    result.points = PolygonOperations::simplifyPolygon(points, tolerance);

    // Simplify holes
    result.children.reserve(children.size());
    for (const auto& hole : children) {
        result.children.push_back(hole.simplify(tolerance));
    }

    // Copy metadata
    result.updateMetadataAfterTransform(*this);

    return result;
}

} // namespace deepnest
