#include "../../include/deepnest/core/Point.h"

namespace deepnest {

BoostPoint Point::toBoost() const {
    return BoostPoint(x, y);
}

Point Point::fromBoost(const BoostPoint& p, bool exact) {
    return Point(boost::polygon::x(p), boost::polygon::y(p), exact);
}

Point Point::fromQt(const QPointF& p, bool exact) {
    return Point(p.x(), p.y(), exact);
}

QPointF Point::toQt() const {
    return QPointF(x, y);
}

} // namespace deepnest
