#include "../../include/deepnest/core/Point.h"
#include <cmath>

namespace deepnest {

BoostPoint Point::toBoost() const {
    return BoostPoint(x, y);
}

Point Point::fromBoost(const BoostPoint& p, bool exact) {
    return Point(boost::polygon::x(p), boost::polygon::y(p), exact);
}

Point Point::fromQt(const QPointF& p, bool exact) {
    // TODO (Phase 4): Add inputScale parameter for proper scaling
    // For now, this is a simple cast with rounding
    // Temporary: assumes coordinates need to be converted to integer space
    return Point(
        static_cast<CoordType>(std::round(p.x())),
        static_cast<CoordType>(std::round(p.y())),
        exact
    );
}

QPointF Point::toQt() const {
    // TODO (Phase 4): Add inputScale parameter for proper descaling
    // For now, this directly converts integer to double
    // Temporary: will need scaling division in Phase 4
    return QPointF(static_cast<double>(x), static_cast<double>(y));
}

} // namespace deepnest
