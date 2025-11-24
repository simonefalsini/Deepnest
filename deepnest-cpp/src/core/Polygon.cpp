#include "../../include/deepnest/core/Polygon.h"
#include "../../include/deepnest/geometry/Transformation.h"
#include "../../include/deepnest/geometry/GeometryUtil.h"
#include "../../include/deepnest/geometry/PolygonOperations.h"
#include "../../include/deepnest/geometry/PolygonHierarchy.h"
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

Polygon::Polygon(Polygon&& other) noexcept
    : points(std::move(other.points))
    , children(std::move(other.children))
    , id(other.id)
    , source(other.source)
    , rotation(other.rotation)
    , quantity(other.quantity)
    , isSheet(other.isSheet)
    , name(std::move(other.name))
{
    // Reset other's primitives to safe defaults if needed
    other.id = -1;
    other.source = -1;
}

Polygon& Polygon::operator=(Polygon&& other) noexcept {
    if (this != &other) {
        points = std::move(other.points);
        children = std::move(other.children);
        id = other.id;
        source = other.source;
        rotation = other.rotation;
        quantity = other.quantity;
        isSheet = other.isSheet;
        name = std::move(other.name);

        // Reset other
        other.id = -1;
        other.source = -1;
    }
    return *this;
}

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

    // Simplify path BEFORE conversion to remove degenerate geometries
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

    // If only one subpath, simple case - no hierarchy needed
    if (subpaths.size() == 1) {
        const QPolygonF& firstSubpath = subpaths.first();

        // Convert QPolygonF to vector<Point>
        std::vector<Point> pathPoints;
        pathPoints.reserve(firstSubpath.size());
        for (const QPointF& qpt : firstSubpath) {
            pathPoints.push_back(Point::fromQt(qpt));
        }

        // Apply Ramer-Douglas-Peucker simplification
        std::vector<Point> simplifiedPoints = GeometryUtil::simplifyPolygon(
            pathPoints,
            2.0,        // tolerance (matches JavaScript config)
            false       // use two-pass approach
        );

        if (simplifiedPoints.size() >= 3) {
            result.points = std::move(simplifiedPoints);
        }

        return result;
    }

    // Multiple subpaths - use PolygonHierarchy::buildTree to automatically
    // determine parent-child relationships based on containment
    // This matches JavaScript toTree() logic from svgnest.js lines 541-591
    
    std::vector<Polygon> allPolygons;
    
    for (int i = 0; i < subpaths.size(); ++i) {
        const QPolygonF& subpath = subpaths[i];

        Polygon poly;
        poly.points.reserve(subpath.size());
        for (const QPointF& pt : subpath) {
            poly.points.push_back(Point::fromQt(pt));
        }

        // Apply simplification to each subpath
        if (poly.points.size() >= 3) {
            std::vector<Point> simplifiedPoints = GeometryUtil::simplifyPolygon(
                poly.points,
                2.0,
                false
            );
            
            if (simplifiedPoints.size() >= 3) {
                poly.points = std::move(simplifiedPoints);
                allPolygons.push_back(poly);
            }
        }
    }

    if (allPolygons.empty()) {
        return result;
    }

    // Build hierarchy using PolygonHierarchy::buildTree
    // This automatically identifies which polygons are holes based on containment
    std::vector<Polygon> tree = PolygonHierarchy::buildTree(allPolygons, polygonId);

    // Return the first top-level polygon (or empty if none)
    if (!tree.empty()) {
        result = tree[0];
        // If polygonId was specified, ensure it's set
        if (polygonId >= 0) {
            result.id = polygonId;
        }
    }

    return result;
}

std::vector<Polygon> Polygon::extractFromQPainterPath(const QPainterPath& path) {
    std::vector<Polygon> polygons;

    // QPainterPath can contain multiple disconnected subpaths
    // We need to extract each one as a separate polygon

    // Apply same cleaning as fromQPainterPath for consistency
    QPainterPath cleanPath = path;
    cleanPath.setFillRule(Qt::WindingFill);
    cleanPath = cleanPath.simplified();
    QList<QPolygonF> qtPolygons = cleanPath.toSubpathPolygons();

    for (const auto& qtPoly : qtPolygons) {
        if (qtPoly.size() < 3) {
            continue;
        }

        // Convert QPolygonF to vector<Point>
        std::vector<Point> pathPoints;
        pathPoints.reserve(qtPoly.size());
        for (const QPointF& qpt : qtPoly) {
            pathPoints.push_back(Point::fromQt(qpt));
        }

        // Apply Ramer-Douglas-Peucker simplification (same as fromQPainterPath)
        std::vector<Point> simplifiedPoints = GeometryUtil::simplifyPolygon(
            pathPoints,
            2.0,        // tolerance (matches JavaScript config)
            false       // use two-pass approach
        );

        if (simplifiedPoints.size() >= 3) {
            Polygon poly;
            poly.points = std::move(simplifiedPoints);

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
