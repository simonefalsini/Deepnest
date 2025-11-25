#include "../../include/deepnest/core/Point.h"
#include <cmath>

namespace deepnest {

BoostPoint Point::toBoost() const {
    return BoostPoint(x, y);
}

Point Point::fromBoost(const BoostPoint& p, bool exact) {
    return Point(boost::polygon::x(p), boost::polygon::y(p), exact);
}

// Deprecated version (no scaling)
Point Point::fromQt(const QPointF& p, bool exact) {
    // WARNING: This deprecated function doesn't apply scaling!
    // Use fromQt(p, scale, exact) instead.
    return Point(
        static_cast<CoordType>(std::round(p.x())),
        static_cast<CoordType>(std::round(p.y())),
        exact
    );
}

// New scaled version
Point Point::fromQt(const QPointF& p, double scale, bool exact) {
    // Convert physical coordinates (double) to scaled integer coordinates
    // Formula: integer_coord = round(physical_coord * scale)
    return Point(
        static_cast<CoordType>(std::round(p.x() * scale)),
        static_cast<CoordType>(std::round(p.y() * scale)),
        exact
    );
}

// Deprecated version (no descaling)
QPointF Point::toQt() const {
    // WARNING: This deprecated function doesn't apply descaling!
    // Use toQt(scale) instead.
    return QPointF(static_cast<double>(x), static_cast<double>(y));
}

// New descaled version
QPointF Point::toQt(double scale) const {
    // Convert scaled integer coordinates to physical coordinates (double)
    // Formula: physical_coord = integer_coord / scale
    return QPointF(
        static_cast<double>(x) / scale,
        static_cast<double>(y) / scale
    );
}

} // namespace deepnest
